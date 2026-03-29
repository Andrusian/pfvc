#include "MainWindow.h"
#include "SettingsDialog.h"
#include "Config.h"
#include "PlaybackBlock.h"
#include <QMenuBar>
#include <QAction>
#include <QLabel>
#include <QCloseEvent>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QTabWidget>
#include <QDebug>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("PipeWire Fine Volume");
    setMinimumSize(500, 400);
    resize(500, 500);

    Config::instance().load();
    buildMenuBar();

    auto* central    = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(central);
    mainLayout->setContentsMargins(4, 4, 4, 4);
    mainLayout->setSpacing(0);

    m_tabs = new QTabWidget(central);
    mainLayout->addWidget(m_tabs);

    buildOutputTab();
    buildPlaybackTab();

    m_tabs->addTab(m_outputTab,   "Output Devices");
    m_tabs->addTab(m_playbackTab, "Playback Streams");

    setCentralWidget(central);

    m_pw = new PipeWireManager(this);
    connect(m_pw, &PipeWireManager::nodeAdded,         this, &MainWindow::onNodeAdded);
    connect(m_pw, &PipeWireManager::nodeRemoved,       this, &MainWindow::onNodeRemoved);
    connect(m_pw, &PipeWireManager::nodeVolumeChanged, this, &MainWindow::onNodeVolumeChanged);
    connect(m_pw, &PipeWireManager::syncDone,          this, &MainWindow::onSyncDone);
    connect(m_pw, &PipeWireManager::nodePeakChanged,   this, &MainWindow::onNodePeakChanged);
    connect(m_pw, &PipeWireManager::nodeRoutingChanged,this, &MainWindow::onNodeRoutingChanged);

    if (!m_pw->start()) {
        qWarning() << "Failed to connect to PipeWire daemon";
    }
}

MainWindow::~MainWindow() = default;

void MainWindow::buildMenuBar() {
    auto* configAction = new QAction("Settings", this);
    connect(configAction, &QAction::triggered, this, &MainWindow::onConfigScreen);
    menuBar()->addAction(configAction);

    auto* muteAllAction = new QAction("Mute All Sinks", this);
    connect(muteAllAction, &QAction::triggered, this, &MainWindow::onMuteAll);
    menuBar()->addAction(muteAllAction);
}

void MainWindow::buildOutputTab() {
    m_outputTab    = new QWidget();
    auto* tabLayout = new QVBoxLayout(m_outputTab);
    tabLayout->setContentsMargins(0, 0, 0, 0);

    m_outputScroll  = new QScrollArea(m_outputTab);
    m_outputContent = new QWidget();
    m_outputLayout  = new QVBoxLayout(m_outputContent);
    m_outputLayout->setContentsMargins(6, 6, 6, 6);
    m_outputLayout->setSpacing(8);
    m_outputLayout->addStretch();

    m_outputScroll->setWidget(m_outputContent);
    m_outputScroll->setWidgetResizable(true);
    m_outputScroll->setFrameShape(QFrame::NoFrame);

    tabLayout->addWidget(m_outputScroll);
}

void MainWindow::onNodeRoutingChanged(uint32_t streamId, uint32_t newTargetId) {
    PlaybackBlock* pb = m_playbackBlocks.value(streamId, nullptr);
    if (!pb) return;
    pb->setCurrentSink(newTargetId);   // no signal emitted — just UI update
}

void MainWindow::buildPlaybackTab() {
    m_playbackTab    = new QWidget();
    auto* tabLayout  = new QVBoxLayout(m_playbackTab);
    tabLayout->setContentsMargins(0, 0, 0, 0);

    auto* scroll     = new QScrollArea(m_playbackTab);
    auto* container  = new QWidget();
    m_playbackLayout = new QVBoxLayout(container);
    m_playbackLayout->setContentsMargins(6, 6, 6, 6);
    m_playbackLayout->setSpacing(0);
    m_playbackLayout->setAlignment(Qt::AlignTop);

    scroll->setWidget(container);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    tabLayout->addWidget(scroll);
}

QList<PwNodeInfo> MainWindow::getEnabledSinks() const {
    QList<PwNodeInfo> active = m_pw->knownSinks();
    QList<PwNodeInfo> enabled;
    for (const auto& info : active) {
        if (Config::instance().streamConfig(info.nodeName).selected) {
            enabled.append(info);
        }
    }
    return enabled;
}

void MainWindow::onNodeAdded(PwNodeInfo info) {
    if (info.mediaClass == "Audio/Sink") {
        if (m_sinkBlocks.contains(info.nodeName)) {
            m_sinkBlocks[info.nodeName]->setPwId(info.id);
            return;
        }

        auto& sc = Config::instance().streamConfig(info.nodeName);
        if (sc.selected) {
            addSinkBlock(info);
            for (auto* pb : m_playbackBlocks) pb->updateSinks(getEnabledSinks());
        }
    } 
    else if (info.mediaClass == "Stream/Output/Audio") {
        auto* block = new PlaybackBlock(info, getEnabledSinks(), m_playbackTab);

        // CONNECT using the standardized name
        connect(block, &PlaybackBlock::routingChanged, 
            this, &MainWindow::onRoutingChanged);
        
        connect(block, &PlaybackBlock::volumeChanged,
                this, [this](uint32_t id, float vol) {
                  m_pw->setNodeVolume(id, vol);
                });
            
        m_playbackLayout->addWidget(block);
        m_playbackBlocks[info.id] = block;
        m_playbackNameToId[info.nodeName] = info.id;
        
        // Set initial routing if we already know it
        if (info.targetId != 0)
          block->setCurrentSink(info.targetId);

    }
}

void MainWindow::onNodeRemoved(uint32_t id, QString nodeName) {
    // 1. Clean up Sinks (keyed by Name)
    if (m_sinkBlocks.contains(nodeName)) {
        StreamBlock* block = m_sinkBlocks.take(nodeName);
        m_outputLayout->removeWidget(block);
        block->deleteLater();
        
        // Refresh all playback blocks because a sink disappeared
        for (auto* pb : m_playbackBlocks) {
            pb->updateSinks(m_pw->knownSinks());
        }
    }

    // 2. Clean up Playback Streams (keyed by numeric ID)
    // This fixes the "cannot convert QString to unsigned int" error
    if (m_playbackBlocks.contains(id)) {
        PlaybackBlock* block = m_playbackBlocks.take(id);
        m_playbackLayout->removeWidget(block);
        block->deleteLater();
        m_playbackNameToId.remove(nodeName);
    }
    
}

// Slot to handle the ComboBox change from a PlaybackBlock
void MainWindow::onRoutingChanged(uint32_t streamId, uint32_t targetSinkId) {
    qDebug() << "Routing stream" << streamId << "to sink" << targetSinkId;
    m_pw->moveStream(streamId, targetSinkId);
}

void MainWindow::onNodeVolumeChanged(uint32_t id, float volume, bool muted) {
    // 1. Check if it's a Playback Stream (Keyed by ID)
    // .value() returns nullptr if the ID isn't found, preventing the crash
    PlaybackBlock* pb = m_playbackBlocks.value(id, nullptr);
    if (pb) {
        pb->updateVolume(volume, muted);
        return; 
    }

    // 2. Check if it's a Sink (Keyed by Name, so we must find the ID)
    for (StreamBlock* sb : m_sinkBlocks.values()) {
        if (sb->pwId() == id) {
            // StreamBlock uses 0-100 scale, PipeWire uses 0.0-1.0
            sb->setVolume(static_cast<int>(std::round(volume * 100.0f)));
            sb->setMuted(muted);
            return;
        }
    }
}


void MainWindow::onNodePeakChanged(QString nodeName, float peakL, float peakR) {
    // Sink blocks
    if (m_sinkBlocks.contains(nodeName)) {
        StreamBlock* block = m_sinkBlocks[nodeName];
        float volumeFactor = block->isMuted() ? 0.0f : (static_cast<float>(block->volume()) / 100.0f);
        int pctL = qBound(0, qRound(peakL * volumeFactor * 100.0f), 100);
        int pctR = qBound(0, qRound(peakR * volumeFactor * 100.0f), 100);
        block->setVuLevel(pctL, pctR);
        return;
    }

    // Playback blocks — looked up by nodeName
    if (m_playbackNameToId.contains(nodeName)) {
        PlaybackBlock* pb = m_playbackBlocks.value(m_playbackNameToId[nodeName], nullptr);
        if (pb) {
            int pctL = qBound(0, qRound(peakL * 100.0f), 100);
            int pctR = qBound(0, qRound(peakR * 100.0f), 100);
            pb->setVuLevel(pctL, pctR);
        }
    }
}


void MainWindow::onSyncDone() {
    qDebug() << "PipeWire sync complete.";
    for (auto it = m_playbackBlocks.begin(); it != m_playbackBlocks.end(); ++it) {
        uint32_t targetId = m_pw->streamTargetId(it.key());
        if (targetId != 0)
            it.value()->setCurrentSink(targetId);
    }
}

void MainWindow::addSinkBlock(const PwNodeInfo& info) {
    auto& sc    = Config::instance().streamConfig(info.nodeName);
    auto* block = new StreamBlock(info.nodeName, info.id, info.name, m_outputContent);
    block->setVolume(qRound(info.volume * 100.0f));
    block->setBlockColor(sc.color);

    connect(block, &StreamBlock::volumeChangeRequested, this, &MainWindow::onVolumeChangeRequested);
    connect(block, &StreamBlock::muteRequested,         this, &MainWindow::onMuteRequested);

    m_outputLayout->insertWidget(m_outputLayout->count() - 1, block);
    m_sinkBlocks[info.nodeName] = block;
}

void MainWindow::onMuteAll() {
    for (auto* block : m_sinkBlocks) onVolumeChangeRequested(block->nodeName(), 0);
}

void MainWindow::onConfigScreen() {
    SettingsDialog dlg(m_pw, this);
    if (dlg.exec() == QDialog::Accepted) {
        Config::instance().save();
        refreshSinkBlocks();
        for (auto* pb : m_playbackBlocks) pb->updateSinks(getEnabledSinks());
    }
}

void MainWindow::onMuteRequested(const QString& nodeName, bool mute) {
    if (!m_sinkBlocks.contains(nodeName)) return;
    m_pw->setNodeMute(m_sinkBlocks[nodeName]->pwId(), mute);
}

void MainWindow::onVolumeChangeRequested(const QString& nodeName, int volume) {
    if (!m_sinkBlocks.contains(nodeName)) return;
    m_pw->setNodeVolume(m_sinkBlocks[nodeName]->pwId(), static_cast<float>(volume) / 100.0f);
}

void MainWindow::refreshSinkBlocks() {
    QList<QString> toRemove;
    for (auto it = m_sinkBlocks.begin(); it != m_sinkBlocks.end(); ++it) {
        if (!Config::instance().streamConfig(it.key()).selected) toRemove.append(it.key());
    }
    for (const QString& name : toRemove) {
        StreamBlock* b = m_sinkBlocks.take(name);
        m_outputLayout->removeWidget(b);
        b->deleteLater();
    }
    for (const PwNodeInfo& info : m_pw->knownSinks()) {
        if (Config::instance().streamConfig(info.nodeName).selected && !m_sinkBlocks.contains(info.nodeName))
            addSinkBlock(info);
    }
}

void MainWindow::closeEvent(QCloseEvent* event) {
    Config::instance().save();
    if (m_pw) m_pw->stop();
    event->accept();
}
