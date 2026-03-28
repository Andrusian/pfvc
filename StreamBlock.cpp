#include "StreamBlock.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QKeyEvent>
#include <QPainter>
#include <QSizePolicy>

StreamBlock::StreamBlock(const QString& nodeName,
                         uint32_t       pwId,
                         const QString& displayName,
                         QWidget*       parent)
    : QWidget(parent), m_nodeName(nodeName), m_pwId(pwId)
{
    setFocusPolicy(Qt::StrongFocus);
    buildUi(displayName);
}


void StreamBlock::setMuted(bool muted) {
    // Update the button text or appearance
    m_muteBtn->setText(muted ? "Unmute" : "Mute");
    
    // Visually "dim" the slider if muted
    // m_vuBar->setEnabled(!muted);
    
    // You could also change the button stylesheet to make it red when muted
    if (muted) {
        m_muteBtn->setStyleSheet("background-color: #552222; color: white;");
    } else {
        m_muteBtn->setStyleSheet("");
    }
}



void StreamBlock::buildUi(const QString& displayName) {
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(6, 6, 6, 6);
    outer->setSpacing(4);

    m_nameLabel = new QLabel(displayName, this);
    m_nameLabel->setStyleSheet("font-weight: bold;");
    outer->addWidget(m_nameLabel);

    auto* row = new QHBoxLayout();
    row->setSpacing(4);

    m_volDisplay = new QLabel("100%", this);
    m_volDisplay->setFixedWidth(52);
    m_volDisplay->setAlignment(Qt::AlignCenter);
    m_volDisplay->setStyleSheet(
        "background-color: #222222; color: #ffff00;"
        "font-weight: bold; padding: 2px; border-radius: 3px;");
    row->addWidget(m_volDisplay);

    auto* col1 = new QVBoxLayout();
    col1->setSpacing(2);
    m_inc1 = new QPushButton("+1", this);
    m_dec1 = new QPushButton("-1", this);
    m_inc1->setFixedSize(34, 22);
    m_dec1->setFixedSize(34, 22);
    col1->addWidget(m_inc1);
    col1->addWidget(m_dec1);
    row->addLayout(col1);

    auto* col2 = new QVBoxLayout();
    col2->setSpacing(2);
    m_inc5 = new QPushButton("+5", this);
    m_dec5 = new QPushButton("-5", this);
    m_inc5->setFixedSize(34, 22);
    m_dec5->setFixedSize(34, 22);
    col2->addWidget(m_inc5);
    col2->addWidget(m_dec5);
    row->addLayout(col2);

    m_boost = new QPushButton("Boost", this);
    m_boost->setFixedHeight(46);
    m_boost->setAutoRepeat(false);
    m_boost->setCheckable(false);
    row->addWidget(m_boost);

    m_muteBtn = new QPushButton("Mute", this);
    m_muteBtn->setFixedHeight(46);
    row->addWidget(m_muteBtn);

    row->addStretch();
    outer->addLayout(row);

    m_vuBar = new VuBar(this);
    outer->addWidget(m_vuBar);

    connect(m_inc1,    &QPushButton::clicked,  this, &StreamBlock::onInc1);
    connect(m_dec1,    &QPushButton::clicked,  this, &StreamBlock::onDec1);
    connect(m_inc5,    &QPushButton::clicked,  this, &StreamBlock::onInc5);
    connect(m_dec5,    &QPushButton::clicked,  this, &StreamBlock::onDec5);
    connect(m_boost,   &QPushButton::pressed,  this, &StreamBlock::onBoostPressed);
    connect(m_boost,   &QPushButton::released, this, &StreamBlock::onBoostReleased);
    connect(m_muteBtn, &QPushButton::clicked,  this, &StreamBlock::onMute);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    applyVolume();
}

void StreamBlock::setVolume(int vol) {
    int clamped = qBound(0, vol, 100);
    if (m_muted) {
        // Keep muted state; just update the restore value
        m_preMute = clamped;
    } else {
        m_volume = clamped;
        applyVolume();
    }
}

void StreamBlock::setBlockColor(const QColor& color) {
    m_blockColor = color;
    update();
}

void StreamBlock::setVuLevel(int levelL, int levelR) {
    m_vuBar->setLevels(levelL, levelR);
}

void StreamBlock::applyVolume() {
    m_volDisplay->setText(QString("%1%").arg(m_volume));
}

void StreamBlock::paintEvent(QPaintEvent* event) {
    QWidget::paintEvent(event);
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), m_blockColor);
    if (m_focused)
        p.setPen(QPen(QColor("#aaaaff"), 2));
    else
        p.setPen(QPen(QColor("#555555"), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void StreamBlock::focusInEvent(QFocusEvent* event) {
    m_focused = true;
    update();
    QWidget::focusInEvent(event);
}

void StreamBlock::focusOutEvent(QFocusEvent* event) {
    m_focused = false;
    update();
    QWidget::focusOutEvent(event);
}

void StreamBlock::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
        case Qt::Key_Up:   onInc1(); break;
        case Qt::Key_Down: onDec1(); break;
        default: QWidget::keyPressEvent(event); break;
    }
}

// ---------------------------------------------------------------------------
// Button slots — all signals carry m_nodeName (stable key)
// ---------------------------------------------------------------------------

void StreamBlock::onInc1() {
    if (m_muted) { m_muted = false; m_muteBtn->setText("Mute"); emit muteRequested(m_nodeName, false); }
    m_volume = qMin(100, m_volume + 1);
    applyVolume();
    emit volumeChangeRequested(m_nodeName, m_volume);
}

void StreamBlock::onDec1() {
    if (m_muted) { m_muted = false; m_muteBtn->setText("Mute"); emit muteRequested(m_nodeName, false); }
    m_volume = qMax(0, m_volume - 1);
    applyVolume();
    emit volumeChangeRequested(m_nodeName, m_volume);
}

void StreamBlock::onInc5() {
    if (m_muted) { m_muted = false; m_muteBtn->setText("Mute"); emit muteRequested(m_nodeName, false); }
    m_volume = qMin(100, m_volume + 5);
    applyVolume();
    emit volumeChangeRequested(m_nodeName, m_volume);
}

void StreamBlock::onDec5() {
    if (m_muted) { m_muted = false; m_muteBtn->setText("Mute"); emit muteRequested(m_nodeName, false); }
    m_volume = qMax(0, m_volume - 5);
    applyVolume();
    emit volumeChangeRequested(m_nodeName, m_volume);
}

void StreamBlock::onBoostPressed() {
    m_preBoost = m_volume;
    m_volume   = qMin(100, m_volume + 5);
    applyVolume();
    emit volumeChangeRequested(m_nodeName, m_volume);
}

void StreamBlock::onBoostReleased() {
    m_volume = m_preBoost;
    applyVolume();
    emit volumeChangeRequested(m_nodeName, m_volume);
}

void StreamBlock::onMute() {
    if (!m_muted) {
        m_preMute = m_volume;
        m_muted   = true;
        m_muteBtn->setText("Unmute");
        m_volDisplay->setText("MUT");
        emit muteRequested(m_nodeName, true);
    } else {
        m_muted  = false;
        m_volume = m_preMute;
        m_muteBtn->setText("Mute");
        applyVolume();
        emit muteRequested(m_nodeName, false);
    }
}
