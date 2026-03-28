#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QCheckBox>
#include "PipeWireManager.h"

class PlaybackBlock : public QWidget {
    Q_OBJECT
public:
    PlaybackBlock(const PwNodeInfo& info, const QList<PwNodeInfo>& sinks, QWidget* parent = nullptr);

    uint32_t pwId() const { return m_id; }
    void updateVolume(float volume, bool muted);
    void updateSinks(const QList<PwNodeInfo>& sinks);
    void setCurrentSink(uint32_t sinkId);

signals:
    void volumeChanged(uint32_t id, float vol);
    void muteToggled(uint32_t id, bool mute);
    void routingChanged(uint32_t id, uint32_t sinkId);

private slots:
    void onSliderMoved(int val);
    void onMuteClicked();
    void onComboChanged(int index);
  

private:
  
    uint32_t m_id;
    float    m_lastVol = 1.0f;
    bool     m_isMuted = false;

    QLabel* m_nameLabel=nullptr;
    QPushButton* m_muteBtn  = nullptr; 
    QCheckBox* m_muteCheck;
    QSlider* m_slider;
    PwNodeInfo m_info;     
    QComboBox* m_sinkCombo;
    QList<uint32_t> m_sinkIds;
};
