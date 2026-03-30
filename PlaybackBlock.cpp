#include "PlaybackBlock.h"
#include <QVBoxLayout>
#include <QHBoxLayout>

PlaybackBlock::PlaybackBlock(const PwNodeInfo& info, const QList<PwNodeInfo>& sinks, QWidget* parent)
    : QWidget(parent), m_id(info.id), m_info(info)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(4, 2, 4, 10);
    outer->setSpacing(2);

    // --- Row 1: Mute button + stream name ---
    auto* row1 = new QHBoxLayout();
    row1->setSpacing(4);

    m_muteBtn = new QPushButton("Mute", this);
    m_muteBtn->setFixedSize(48, 22);
    m_muteBtn->setStyleSheet("background-color: #994444;");

    row1->addWidget(m_muteBtn);

    m_nameLabel = new QLabel(info.name, this);
    m_nameLabel->setStyleSheet("font-weight: bold;");
    row1->addWidget(m_nameLabel, 1);

    outer->addLayout(row1);

    // --- Row 2: Destination sink selector ---
    m_sinkCombo = new QComboBox(this);
    m_sinkCombo->setStyleSheet("background-color: #226666;");
    outer->addWidget(m_sinkCombo);
    
    // --- Row 3: VU bar — same width as combo box ---
    m_vuBar = new VuBar(this);
    outer->addWidget(m_vuBar);

    updateSinks(sinks);

    connect(m_sinkCombo, &QComboBox::activated, this, [this](int index) {
        if (index < 0) return;
        uint32_t targetId = m_sinkCombo->itemData(index).toUInt();
        emit routingChanged(m_info.id, targetId);
    });

    connect(m_muteBtn, &QPushButton::clicked, this, [this]() {
        m_isMuted = !m_isMuted;
        m_muteBtn->setText(m_isMuted ? "Unmute" : "Mute");
        m_muteBtn->setStyleSheet(m_isMuted ? "background-color: #552222; color: white;" : "");
        emit volumeChanged(m_id, m_isMuted ? 0.0f : m_lastVol);
    });

    

}

void PlaybackBlock::updateSinks(const QList<PwNodeInfo>& sinks) {
    if (!m_sinkCombo) return;

    uint32_t currentId = 0;
    if (m_sinkCombo->currentIndex() >= 0)
        currentId = m_sinkCombo->itemData(m_sinkCombo->currentIndex()).toUInt();

    m_sinkCombo->blockSignals(true);
    m_sinkCombo->clear();

    for (const auto& sink : sinks) {
        m_sinkCombo->addItem(sink.name, sink.id);
        if (sink.id == currentId)
            m_sinkCombo->setCurrentIndex(m_sinkCombo->count() - 1);
    }

    if (m_sinkCombo->currentIndex() < 0 && m_sinkCombo->count() > 0)
        m_sinkCombo->setCurrentIndex(0);

    m_sinkCombo->blockSignals(false);
}

void PlaybackBlock::setCurrentSink(uint32_t sinkId) {
    if (!m_sinkCombo) return;
    m_sinkCombo->blockSignals(true);
    for (int i = 0; i < m_sinkCombo->count(); ++i) {
        if (m_sinkCombo->itemData(i).toUInt() == sinkId) {
            m_sinkCombo->setCurrentIndex(i);
            break;
        }
    }
    m_sinkCombo->blockSignals(false);
}

void PlaybackBlock::setVuLevel(int levelL, int levelR) {
    if (m_vuBar)
        m_vuBar->setLevels(levelL, levelR);
}

void PlaybackBlock::updateVolume(float volume, bool muted) {
    if (!m_nameLabel) return;
    if (!m_isMuted && volume > 0.0f)
        m_lastVol = volume;
}

void PlaybackBlock::onSliderMoved(int val) {
    emit volumeChanged(m_id, val / 100.0f);
}

void PlaybackBlock::onMuteClicked() {
    emit muteToggled(m_id, m_muteCheck->isChecked());
}

void PlaybackBlock::onComboChanged(int index) {
    if (index >= 0 && index < m_sinkIds.size())
        emit routingChanged(m_id, m_sinkIds[index]);
}
