#pragma once
#include <QObject>
#include <QSocketNotifier>
#include <QTimer>
#include <QString>
#include <QMap>
#include <atomic>

#include <pipewire/pipewire.h>
#include <pipewire/extensions/metadata.h>

struct PwNodeInfo {
    uint32_t id         = 0;
    uint32_t targetId   = 0;   // current routing target (sink node id)
    QString  name;
    QString  nodeName;
    QString  mediaClass;
    float    volume     = 1.0f;
    bool     muted      = false;
    uint32_t nChannels  = 0;
};

class PipeWireManager : public QObject {
    Q_OBJECT

public:
    explicit PipeWireManager(QObject* parent = nullptr);
    ~PipeWireManager() override;

    bool start();
    void stop();

    void setNodeVolume(uint32_t id, float volume);
    void setNodeMute(uint32_t id, bool mute);
    void moveStream(uint32_t streamId, uint32_t targetSinkId);

    QList<PwNodeInfo> knownSinks() const;

    // Returns the current routing target sink id for a playback stream
    uint32_t streamTargetId(uint32_t streamId) const;

signals:
    void nodeAdded(PwNodeInfo info);
    void nodeVolumeChanged(uint32_t id, float volume, bool muted);
    void nodePeakChanged(QString nodeName, float peakL, float peakR);
    void nodeRemoved(uint32_t id, QString nodeName);
    void nodeRoutingChanged(uint32_t streamId, uint32_t newTargetId);
    void syncDone();

private:
    struct pw_core_events     m_coreEvents{};
    struct pw_registry_events m_regEvents{};

    // Metadata for tracking routing changes
    struct pw_metadata*        m_metadata         = nullptr;
    struct spa_hook            m_metadataListener{};
    struct pw_metadata_events  m_metadataEvents{};
    QString                    m_defaultSinkName;

    void onPipeWireFdReady();
    void onPeakTimer();

    static void onRegistryGlobal(void* data, uint32_t id, uint32_t permissions,
                                  const char* type, uint32_t version,
                                  const struct spa_dict* props);
    static void onRegistryGlobalRemove(void* data, uint32_t id);
    static void onNodeInfo(void* data, const struct pw_node_info* info);
    static void onNodeParam(void* data, int seq, uint32_t id, uint32_t index,
                             uint32_t next, const struct spa_pod* param);
    static void onStreamProcess(void* data);
    static void onStreamStateChanged(void* data, enum pw_stream_state old,
                                      enum pw_stream_state cur, const char* error);
    static void onCoreDone(void* data, uint32_t id, int seq);
    static int  onMetadataProperty(void* data, uint32_t subject,
                                    const char* key, const char* type,
                                    const char* value);

    struct NodeProxy {
        uint32_t id = 0;
        struct pw_node* proxy = nullptr;
        struct spa_hook nodeListener{};
        PipeWireManager* mgr = nullptr;
        PwNodeInfo info;
        bool announced = false;
        struct pw_stream* monStream = nullptr;
        struct pw_stream_events monStreamEvents{};
        struct spa_hook monListener{};
        std::atomic<float> peakL{0.0f};
        std::atomic<float> peakR{0.0f};
    };

    void subscribeToNode(uint32_t id, const struct spa_dict* props);
    void createMonitorStream(NodeProxy* np);
    void destroyNodeProxy(uint32_t id);

    QMap<uint32_t, NodeProxy*> m_nodes;

    struct pw_context*   m_context  = nullptr;
    struct pw_core*      m_core     = nullptr;
    struct pw_registry*  m_registry = nullptr;
    struct pw_main_loop* m_mainLoop = nullptr;
    struct pw_loop*      m_loop     = nullptr;

    struct spa_hook      m_registryListener{};
    struct spa_hook      m_coreListener{};
    int                  m_syncSeq  = 0;
    QSocketNotifier*     m_notifier = nullptr;
    QTimer*              m_peakTimer = nullptr;
};
