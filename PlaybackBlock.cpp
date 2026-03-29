#include "PlaybackBlock.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <iostream>

PlaybackBlock::PlaybackBlock(const PwNodeInfo& info, const QList<PwNodeInfo>& sinks, QWidget* parent)
    : QWidget(parent), m_id(info.id), m_info(info)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(4, 2, 4, 2);
    outer->setSpacing(2);

    // --- Row 1: Mute button + stream name ---
    auto* row1 = new QHBoxLayout();
    row1->setSpacing(4);

    m_muteBtn = new QPushButton("Mute", this);
    m_muteBtn->setFixedSize(60, 25);
    row1->addWidget(m_muteBtn);

    m_nameLabel = new QLabel(info.name, this);
    m_nameLabel->setStyleSheet("font-weight: bold;");
    row1->addWidget(m_nameLabel, 1);

    outer->addLayout(row1);

    // --- Row 2: Destination sink selector ---
    m_sinkCombo = new QComboBox(this);
    outer->addWidget(m_sinkCombo);

    updateSinks(sinks);

    connect(m_sinkCombo, &QComboBox::activated, this, [this](int index) {
        if (index < 0) return;
        uint32_t targetId = m_sinkCombo->itemData(index).toUInt();
        emit routingChanged(m_info.id, targetId);
    });

    connect(m_muteBtn, &QPushButton::clicked, this, [this]() {
        m_isMuted = !m_isMuted;
        m_muteBtn->setText(m_isMuted ? "Unmute" : "Mute  ");
        m_muteBtn->setStyleSheet(m_isMuted ? "background-color: #552222; color: white;" : "");
        emit volumeChanged(m_id, m_isMuted ? 0.0f : m_lastVol);
    });
}

// PlaybackBlock::PlaybackBlock(const PwNodeInfo& info, const QList<PwNodeInfo>& sinks, QWidget* parent)
//     : QWidget(parent), m_id(info.id), m_info(info) {   // ← initialize m_info here
//     auto* layout = new QHBoxLayout(this);
//     layout->setContentsMargins(4, 2, 4, 2);

//     m_nameLabel = new QLabel(info.name, this);          // ← use info.name directly
//     layout->addWidget(m_nameLabel, 1);

//     m_sinkCombo = new QComboBox(this);
//     layout->addWidget(m_sinkCombo, 1);

//     updateSinks(sinks);

//     connect(m_sinkCombo, &QComboBox::activated, this, [this](int index) {
//         if (index < 0) return;
//         uint32_t targetId = m_sinkCombo->itemData(index).toUInt();
//         emit routingChanged(m_info.id, targetId);
//     });
// }


void PlaybackBlock::updateSinks(const QList<PwNodeInfo>& sinks) {
    if (!m_sinkCombo) return;

    // Save selection safely
    uint32_t currentId = 0;
    if (m_sinkCombo->currentIndex() >= 0) {
        currentId = m_sinkCombo->itemData(m_sinkCombo->currentIndex()).toUInt();
    }

    m_sinkCombo->blockSignals(true);
    m_sinkCombo->clear();

    for (const auto& sink : sinks) {
        m_sinkCombo->addItem(sink.name, sink.id);
        if (sink.id == currentId) {
            m_sinkCombo->setCurrentIndex(m_sinkCombo->count() - 1);
        }
    }
    
    // If nothing was selected before, try to match the "default" (placeholder logic)
    if (m_sinkCombo->currentIndex() < 0 && m_sinkCombo->count() > 0) {
        m_sinkCombo->setCurrentIndex(0);
    }

    m_sinkCombo->blockSignals(false);
}


void PlaybackBlock::updateVolume(float volume, bool muted) {
    // Safety check: The stack trace showed a crash here if 'this' was null
    // or if internal UI elements weren't ready.

  std::cout << "New volume " << volume << " muted " << muted << "\n";

  
    if (!m_nameLabel) return;
    if (!m_isMuted && volume > 0.0f)
        m_lastVol = volume;   // remember last non-zero volume for unmute restore

    // Optional: If you add a volume slider or label to PlaybackBlock later,
    // you would update them here. For now, we just ensure the call is safe.
    
    // Example if you had a volume label:
    // int volPercent = static_cast<int>(std::round(volume * 100.0f));
    // m_nameLabel->setText(QString("%1 (%2%)").arg(m_info.name).arg(volPercent));
    
    // If you add a mute checkbox:
    // m_muteCheck->blockSignals(true);
    // m_muteCheck->setChecked(muted);
    // m_muteCheck->blockSignals(false);
}

void PlaybackBlock::onSliderMoved(int val) {
    emit volumeChanged(m_id, val / 100.0f);
}

void PlaybackBlock::onMuteClicked() {
    emit muteToggled(m_id, m_muteCheck->isChecked());
}

void PlaybackBlock::onComboChanged(int index) {
    if (index >= 0 && index < m_sinkIds.size()) {
        emit routingChanged(m_id, m_sinkIds[index]);
    }
}

void PlaybackBlock::setCurrentSink(uint32_t sinkId) {
    m_sinkCombo->blockSignals(true);
    for (int i = 0; i < m_sinkCombo->count(); ++i) {
        if (m_sinkCombo->itemData(i).toUInt() == sinkId) {
            m_sinkCombo->setCurrentIndex(i);
            break;
        }
    }
    m_sinkCombo->blockSignals(false);
}
