#pragma once
#include <QMainWindow>
#include <QMap>
#include <QString>
#include "PipeWireManager.h"
#include "StreamBlock.h"
#include "PlaybackBlock.h"

class QTabWidget;
class QVBoxLayout;
class QScrollArea;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void onNodeAdded(PwNodeInfo info);
    void onNodeRemoved(uint32_t id, QString nodeName);
    void onNodeVolumeChanged(uint32_t id, float volume, bool muted);
    void onNodePeakChanged(QString nodeName, float peakL, float peakR);
    void onSyncDone();
    void onRoutingChanged(uint32_t streamId, uint32_t targetSinkId);
    void onNodeRoutingChanged(uint32_t streamId, uint32_t newTargetId);
    
    void onVolumeChangeRequested(const QString& nodeName, int volume);
    void onMuteRequested(const QString& nodeName, bool mute);
    void onConfigScreen();
    void onMuteAll();

private:
    void buildMenuBar();
    void buildOutputTab();
    void buildPlaybackTab();
    void addSinkBlock(const PwNodeInfo& info);
    void refreshSinkBlocks();
    
    // ADDED DECLARATION:
    QList<PwNodeInfo> getEnabledSinks() const;

    PipeWireManager* m_pw;
    QTabWidget* m_tabs;

    // Output Tab
    QWidget* m_outputTab;
    QScrollArea* m_outputScroll;
    QWidget* m_outputContent;
    QVBoxLayout* m_outputLayout;
    QMap<QString, StreamBlock*> m_sinkBlocks;

    // Playback Tab
    QWidget* m_playbackTab;
    QVBoxLayout* m_playbackLayout;
    QMap<uint32_t, PlaybackBlock*> m_playbackBlocks;

protected:
    void closeEvent(QCloseEvent* event) override;
};
