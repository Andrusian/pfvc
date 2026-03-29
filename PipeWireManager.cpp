#include "PipeWireManager.h"
#include <QDebug>
#include <QProcess>
#include <QRegularExpression>
#include <cmath>

#include <pipewire/pipewire.h>
#include <pipewire/extensions/metadata.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <spa/pod/builder.h>
#include <spa/pod/iter.h>
#include <spa/pod/parser.h>
#include <spa/utils/result.h>

static QString nodeDisplayName(const struct spa_dict* props) {
    const char* desc = spa_dict_lookup(props, "node.description");
    if (desc && *desc) return QString::fromUtf8(desc);
    const char* name = spa_dict_lookup(props, "node.name");
    if (name && *name) return QString::fromUtf8(name);
    return "(unknown)";
}

static bool isInteresting(const char* mediaClass) {
    if (!mediaClass) return false;
    return (qstrcmp(mediaClass, "Audio/Sink") == 0 ||
            qstrcmp(mediaClass, "Stream/Output/Audio") == 0);
}

PipeWireManager::PipeWireManager(QObject* parent)
    : QObject(parent)
{}

PipeWireManager::~PipeWireManager() {
    stop();
}

bool PipeWireManager::start() {
    pw_init(nullptr, nullptr);

    m_mainLoop = pw_main_loop_new(nullptr);
    if (!m_mainLoop) {
        qWarning() << "PipeWireManager: failed to create main loop";
        return false;
    }
    m_loop = pw_main_loop_get_loop(m_mainLoop);

    m_context = pw_context_new(m_loop, nullptr, 0);
    if (!m_context) {
        qWarning() << "PipeWireManager: failed to create context";
        return false;
    }

    m_core = pw_context_connect(m_context, nullptr, 0);
    if (!m_core) {
        qWarning() << "PipeWireManager: failed to connect to PipeWire daemon";
        return false;
    }

    m_coreEvents = {};
    m_coreEvents.version = PW_VERSION_CORE_EVENTS;
    m_coreEvents.done    = &PipeWireManager::onCoreDone;
    pw_core_add_listener(m_core, &m_coreListener, &m_coreEvents, this);

    m_registry = pw_core_get_registry(m_core, PW_VERSION_REGISTRY, 0);
    if (!m_registry) {
        qWarning() << "PipeWireManager: failed to get registry";
        return false;
    }

    m_regEvents = {};
    m_regEvents.version       = PW_VERSION_REGISTRY_EVENTS;
    m_regEvents.global        = &PipeWireManager::onRegistryGlobal;
    m_regEvents.global_remove = &PipeWireManager::onRegistryGlobalRemove;
    pw_registry_add_listener(m_registry, &m_registryListener, &m_regEvents, this);

    m_syncSeq = pw_core_sync(m_core, PW_ID_CORE, 0);

    int fd = pw_loop_get_fd(m_loop);
    m_notifier = new QSocketNotifier(fd, QSocketNotifier::Read, this);
    connect(m_notifier, &QSocketNotifier::activated,
            this, &PipeWireManager::onPipeWireFdReady);

    m_peakTimer = new QTimer(this);
    m_peakTimer->setInterval(100);
    connect(m_peakTimer, &QTimer::timeout,
            this, &PipeWireManager::onPeakTimer);
    m_peakTimer->start();

    return true;
}

void PipeWireManager::stop() {
    if (m_peakTimer) {
        m_peakTimer->stop();
        m_peakTimer = nullptr;
    }
    if (m_notifier) {
        m_notifier->setEnabled(false);
        delete m_notifier;
        m_notifier = nullptr;
    }

    const QList<uint32_t> ids = m_nodes.keys();
    for (uint32_t id : ids)
        destroyNodeProxy(id);
    m_nodes.clear();

    if (m_metadata) {
        spa_hook_remove(&m_metadataListener);
        pw_proxy_destroy(reinterpret_cast<struct pw_proxy*>(m_metadata));
        m_metadata = nullptr;
    }
    if (m_registry) {
        pw_proxy_destroy(reinterpret_cast<struct pw_proxy*>(m_registry));
        m_registry = nullptr;
    }
    if (m_core) {
        pw_core_disconnect(m_core);
        m_core = nullptr;
    }
    if (m_context) {
        pw_context_destroy(m_context);
        m_context = nullptr;
    }
    if (m_mainLoop) {
        pw_main_loop_destroy(m_mainLoop);
        m_mainLoop = nullptr;
    }
    pw_deinit();
}

QList<PwNodeInfo> PipeWireManager::knownSinks() const {
    QList<PwNodeInfo> result;
    for (auto* np : m_nodes) {
        if (np->info.mediaClass == "Audio/Sink")
            result.append(np->info);
    }
    return result;
}

uint32_t PipeWireManager::streamTargetId(uint32_t streamId) const {
    auto* np = m_nodes.value(streamId, nullptr);
    return np ? np->info.targetId : 0;
}

void PipeWireManager::setNodeVolume(uint32_t id, float volume) {
    float clamped = qBound(0.0f, volume, 1.0f);
    QProcess::startDetached("wpctl",
        { "set-volume",
          QString::number(id),
          QString::number(static_cast<double>(clamped), 'f', 4) });
}

void PipeWireManager::setNodeMute(uint32_t id, bool mute) {
    QProcess::startDetached("wpctl",
        { "set-mute",
          QString::number(id),
          mute ? "1" : "0" });
}

void PipeWireManager::moveStream(uint32_t streamId, uint32_t targetSinkId) {
    QProcess::startDetached("pw-metadata",
        { QString::number(streamId),
          "target.node",
          QString::number(targetSinkId) });
}

void PipeWireManager::onPipeWireFdReady() {
    pw_loop_iterate(m_loop, 0);
}

void PipeWireManager::onPeakTimer() {
    for (auto it = m_nodes.begin(); it != m_nodes.end(); ++it) {
        NodeProxy* np = it.value();
        float peakL = np->peakL.exchange(0.0f);
        float peakR = np->peakR.exchange(0.0f);
        emit nodePeakChanged(np->info.nodeName, peakL, peakR);
    }
}

void PipeWireManager::onRegistryGlobal(void* data, uint32_t id, uint32_t permissions,
                                        const char* type, uint32_t version,
                                        const struct spa_dict* props)
{
    auto* self = static_cast<PipeWireManager*>(data);
    (void)permissions;
    (void)version;

    // Handle Metadata objects — must come before the Node-only early return
    if (qstrcmp(type, PW_TYPE_INTERFACE_Metadata) == 0) {
        const char* mdName = props ? spa_dict_lookup(props, "metadata.name") : nullptr;
        qDebug() << "Registry: Metadata id=" << id << "name=" << (mdName ? mdName : "null");
        if (mdName && qstrcmp(mdName, "default") == 0 && !self->m_metadata) {
            self->m_metadata = static_cast<struct pw_metadata*>(
                pw_registry_bind(self->m_registry, id,
                                 PW_TYPE_INTERFACE_Metadata,
                                 PW_VERSION_METADATA, 0));
            if (self->m_metadata) {
                self->m_metadataEvents = {};
                self->m_metadataEvents.version  = PW_VERSION_METADATA_EVENTS;
                self->m_metadataEvents.property = &PipeWireManager::onMetadataProperty;
                pw_metadata_add_listener(self->m_metadata,
                                         &self->m_metadataListener,
                                         &self->m_metadataEvents, self);
                qDebug() << "  → default metadata bound successfully";
            } else {
                qWarning() << "  → failed to bind default metadata";
            }
        }
        return;
    }

    // From here on, only handle Nodes
    if (qstrcmp(type, PW_TYPE_INTERFACE_Node) != 0) {
        return;
    }

    const char* mediaClass = spa_dict_lookup(props, PW_KEY_MEDIA_CLASS);
    if (!isInteresting(mediaClass)) {
        return;
    }

    const char* appName = spa_dict_lookup(props, PW_KEY_APP_NAME);
    if (appName && QString::fromUtf8(appName) == "PipeWire Fine Volume") {
        return;
    }

    self->subscribeToNode(id, props);
}

void PipeWireManager::onRegistryGlobalRemove(void* data, uint32_t id) {
    auto* self = static_cast<PipeWireManager*>(data);
    if (!self->m_nodes.contains(id)) return;

    QString nodeName = self->m_nodes[id]->info.nodeName;

    self->destroyNodeProxy(id);
    self->m_nodes.remove(id);

    emit self->nodeRemoved(id, nodeName);
}

void PipeWireManager::onCoreDone(void* data, uint32_t id, int seq) {
    auto* self = static_cast<PipeWireManager*>(data);
    if (id == PW_ID_CORE && seq == self->m_syncSeq)
        emit self->syncDone();
}

int PipeWireManager::onMetadataProperty(void* data, uint32_t subject,
                                         const char* key, const char* type,
                                         const char* value)
{
    auto* self = static_cast<PipeWireManager*>(data);
    (void)type;

    if (!key) return 0;

    // Track the default sink name so we can resolve -1 routing
    if (qstrcmp(key, "default.audio.sink") == 0 && value) {
        // value is JSON: {"name":"alsa_output.xxx"}
        QRegularExpression re("\"name\"\\s*:\\s*\"([^\"]+)\"");
        auto match = re.match(QString::fromUtf8(value));
        if (match.hasMatch()) {
            self->m_defaultSinkName = match.captured(1);
            qDebug() << "Default sink updated to:" << self->m_defaultSinkName;
        }
        return 0;
    }

    if (qstrcmp(key, "target.node") != 0) return 0;

    int parsed = value ? atoi(value) : -1;
    qDebug() << "META target.node subject=" << subject << "value=" << (value ? value : "null") << "parsed=" << parsed;

    if (!self->m_nodes.contains(subject)) return 0;
    NodeProxy* np = self->m_nodes[subject];
    if (np->info.mediaClass != "Stream/Output/Audio") return 0;

    uint32_t targetId = 0;

    if (parsed <= 0) {
        // -1 means "route to default sink"
        for (const auto& sink : self->knownSinks()) {
            if (sink.nodeName == self->m_defaultSinkName) {
                targetId = sink.id;
                break;
            }
        }
        // Fall back to first known sink if default not found
        if (targetId == 0) {
            QList<PwNodeInfo> sinks = self->knownSinks();
            if (!sinks.isEmpty())
                targetId = sinks.first().id;
        }
        qDebug() << "  → resolved -1 to default sink id=" << targetId
                 << "(" << self->m_defaultSinkName << ")";
    } else {
        targetId = static_cast<uint32_t>(parsed);
    }

    if (targetId == 0) return 0;

    // Always emit — don't guard on targetId change since the previous
    // value may have been stale from startup
    np->info.targetId = targetId;
    qDebug() << "  → emitting nodeRoutingChanged" << subject << "->" << targetId;
    emit self->nodeRoutingChanged(subject, targetId);

    return 0;
}

void PipeWireManager::subscribeToNode(uint32_t id, const struct spa_dict* props) {
    auto* np            = new NodeProxy();
    np->id              = id;
    np->mgr             = this;
    np->info.id         = id;
    np->info.name       = nodeDisplayName(props);

    const char* nn      = props ? spa_dict_lookup(props, "node.name") : nullptr;
    np->info.nodeName   = (nn && *nn) ? QString::fromUtf8(nn) : QString::number(id);
    np->info.mediaClass = QString::fromUtf8(spa_dict_lookup(props, "media.class") ?: "");
    np->info.volume     = 1.0f;
    np->info.muted      = false;
    np->info.targetId   = 0;
    np->peakL.store(0.0f);
    np->peakR.store(0.0f);

    np->proxy = static_cast<struct pw_node*>(
        pw_registry_bind(m_registry, id, PW_TYPE_INTERFACE_Node, PW_VERSION_NODE, 0));
    if (!np->proxy) { delete np; return; }

    static struct pw_node_events nodeEvents = {};
    nodeEvents.version = PW_VERSION_NODE_EVENTS;
    nodeEvents.info    = &PipeWireManager::onNodeInfo;
    nodeEvents.param   = &PipeWireManager::onNodeParam;
    pw_node_add_listener(np->proxy, &np->nodeListener, &nodeEvents, np);

    uint32_t paramIds[] = { SPA_PARAM_Props };
    pw_node_subscribe_params(np->proxy, paramIds, 1);

    m_nodes[id] = np;

    if (np->info.mediaClass == "Audio/Sink" ||
        np->info.mediaClass == "Stream/Output/Audio") {
      createMonitorStream(np);
    }
}

void PipeWireManager::createMonitorStream(NodeProxy* np) {
    struct pw_properties* props = pw_properties_new(
        PW_KEY_MEDIA_TYPE,     "Audio",
        PW_KEY_MEDIA_CATEGORY, "Monitor",
        PW_KEY_MEDIA_ROLE,     "Music",
        PW_KEY_TARGET_OBJECT,  qPrintable(np->info.nodeName),
        nullptr);

    const QByteArray streamName = ("pfvc-mon:" + np->info.nodeName).toUtf8();

    np->monStream = pw_stream_new(m_core, streamName.constData(), props);

    if (!np->monStream) {
        qWarning() << "Failed to create monitor stream for node" << np->info.nodeName;
        if (props) pw_properties_free(props);
        return;
    }

    np->monStreamEvents               = {};
    np->monStreamEvents.version       = PW_VERSION_STREAM_EVENTS;
    np->monStreamEvents.state_changed = &PipeWireManager::onStreamStateChanged;
    np->monStreamEvents.process       = &PipeWireManager::onStreamProcess;
    pw_stream_add_listener(np->monStream, &np->monListener, &np->monStreamEvents, np);

    uint8_t buf[1024];
    struct spa_pod_builder b = SPA_POD_BUILDER_INIT(buf, sizeof(buf));
    const struct spa_pod* params[1];
    struct spa_audio_info_raw audioInfo = {};
    audioInfo.format   = SPA_AUDIO_FORMAT_F32;
    audioInfo.channels = 2;
    audioInfo.rate     = 48000;
    params[0] = (const struct spa_pod*) spa_format_audio_raw_build(&b, SPA_PARAM_EnumFormat, &audioInfo);

    pw_stream_connect(np->monStream,
                      PW_DIRECTION_INPUT,
                      PW_ID_ANY,
                      static_cast<enum pw_stream_flags>(
                          PW_STREAM_FLAG_AUTOCONNECT |
                          PW_STREAM_FLAG_MAP_BUFFERS  |
                          PW_STREAM_FLAG_RT_PROCESS),
                      params, 1);
}

void PipeWireManager::destroyNodeProxy(uint32_t id) {
    NodeProxy* np = m_nodes.value(id, nullptr);
    if (!np) return;

    if (np->monStream) {
        spa_hook_remove(&np->monListener);
        pw_stream_disconnect(np->monStream);
        pw_stream_destroy(np->monStream);
        np->monStream = nullptr;
    }
    spa_hook_remove(&np->nodeListener);
    pw_proxy_destroy(reinterpret_cast<struct pw_proxy*>(np->proxy));
    np->proxy = nullptr;
    delete np;
}

void PipeWireManager::onNodeInfo(void* data, const struct pw_node_info* info) {
    auto* np = static_cast<NodeProxy*>(data);
    if (info->props) {
        const char* desc = spa_dict_lookup(info->props, "node.description");
        if (desc && *desc) np->info.name = QString::fromUtf8(desc);

        const char* nn = spa_dict_lookup(info->props, "node.name");
        if (nn && *nn && np->info.nodeName == QString::number(np->id))
            np->info.nodeName = QString::fromUtf8(nn);
    }
    if (!np->announced) {
        np->announced = true;
        emit np->mgr->nodeAdded(np->info);
    }
}

void PipeWireManager::onNodeParam(void* data, int, uint32_t id, uint32_t, uint32_t, const struct spa_pod* param) {
    if (id != SPA_PARAM_Props || !param) return;
    auto* np = static_cast<NodeProxy*>(data);

    float volume = np->info.volume;
    bool  muted  = np->info.muted;

    struct spa_pod_prop* prop;
    SPA_POD_OBJECT_FOREACH(reinterpret_cast<const struct spa_pod_object*>(param), prop) {
        switch (prop->key) {
        case SPA_PROP_mute: {
            bool m = false;
            if (spa_pod_get_bool(&prop->value, &m) == 0) muted = m;
            break;
        }
        case SPA_PROP_channelVolumes: {
            uint32_t nVals = 0;
            float* vals = static_cast<float*>(spa_pod_get_array(&prop->value, &nVals));
            if (vals && nVals > 0) {
                float sum = 0.f;
                for (uint32_t i = 0; i < nVals; ++i) sum += vals[i];
                volume = std::cbrt(sum / static_cast<float>(nVals));
            }
            break;
        }
        default: break;
        }
    }

    np->info.volume = volume;
    np->info.muted  = muted;

    if (!np->announced) {
        np->announced = true;
        emit np->mgr->nodeAdded(np->info);
    } else {
        emit np->mgr->nodeVolumeChanged(np->id, volume, muted);
    }
}

void PipeWireManager::onStreamProcess(void* data) {
    auto* np = static_cast<NodeProxy*>(data);
    struct pw_buffer* pwBuf = pw_stream_dequeue_buffer(np->monStream);
    if (!pwBuf) return;

    struct spa_buffer* spaBuf = pwBuf->buffer;
    if (!spaBuf || !spaBuf->datas) {
        pw_stream_queue_buffer(np->monStream, pwBuf);
        return;
    }

    float peakL = 0.0f, peakR = 0.0f;
    if (spaBuf->n_datas == 1) {
        auto* ptr = static_cast<float*>(spaBuf->datas[0].data);
        if (ptr) {
            uint32_t nSamples = spaBuf->datas[0].chunk->size / sizeof(float);
            for (uint32_t i = 0; i + 1 < nSamples; i += 2) {
                peakL = std::max(peakL, std::fabs(ptr[i]));
                peakR = std::max(peakR, std::fabs(ptr[i + 1]));
            }
        }
    } else {
        for (uint32_t d = 0; d < spaBuf->n_datas && d < 2; ++d) {
            auto* ptr = static_cast<float*>(spaBuf->datas[d].data);
            if (!ptr) continue;
            uint32_t nSamples = spaBuf->datas[d].chunk->size / sizeof(float);
            float p = 0.0f;
            for (uint32_t i = 0; i < nSamples; ++i) p = std::max(p, std::fabs(ptr[i]));
            if (d == 0) peakL = p; else peakR = p;
        }
    }

    auto updatePeak = [](std::atomic<float>& atom, float val) {
        float prev = atom.load();
        while (val > prev && !atom.compare_exchange_weak(prev, val));
    };
    updatePeak(np->peakL, peakL);
    updatePeak(np->peakR, peakR);

    pw_stream_queue_buffer(np->monStream, pwBuf);
}

void PipeWireManager::onStreamStateChanged(void* data, enum pw_stream_state old_state,
                                            enum pw_stream_state new_state, const char* error) {
    auto* np = static_cast<NodeProxy*>(data);
    qDebug() << "Monitor stream" << np->info.nodeName
             << "state:" << pw_stream_state_as_string(old_state)
             << "->" << pw_stream_state_as_string(new_state);

    if (new_state == PW_STREAM_STATE_ERROR && error) {
        qWarning() << "Monitor stream error:" << error;
    }
}
