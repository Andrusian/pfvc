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
    m_muted = muted;
    if (muted) {
        m_volDisplay->setText("MUT");
    } else {
        applyVolume();
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

    // Volume readout
    m_volDisplay = new QLabel("100%", this);
    m_volDisplay->setFixedWidth(52);
    m_volDisplay->setAlignment(Qt::AlignCenter);
    m_volDisplay->setStyleSheet(
        "background-color: #222222; color: #ffff00;"
        "font-weight: bold; padding: 2px; border-radius: 3px;");
    row->addWidget(m_volDisplay);

    // Zero button — momentary, sets volume to 0
    m_zeroBtn = new QPushButton("Zero", this);
    m_zeroBtn->setFixedSize(42, 46);
    m_zeroBtn->setStyleSheet("background-color: #444444; color: white;");
    row->addWidget(m_zeroBtn);

    // +1 / -1 buttons — twice as wide
    auto* col1 = new QVBoxLayout();
    col1->setSpacing(2);
    m_inc1 = new QPushButton("+1", this);
    m_dec1 = new QPushButton("-1", this);
    m_inc1->setFixedSize(68, 22);
    m_dec1->setFixedSize(68, 22);
    m_inc1->setStyleSheet("background-color: #2d6e2d; color: white; font-weight: bold;");
    m_dec1->setStyleSheet("background-color: #8b4500; color: white; font-weight: bold;");
    col1->addWidget(m_inc1);
    col1->addWidget(m_dec1);
    row->addLayout(col1);

    // +5 / -5 buttons
    auto* col2 = new QVBoxLayout();
    col2->setSpacing(2);
    m_inc5 = new QPushButton("+5", this);
    m_dec5 = new QPushButton("-5", this);
    m_inc5->setFixedSize(34, 22);
    m_dec5->setFixedSize(34, 22);
    m_inc5->setStyleSheet("background-color: #2d6e2d; color: white; font-weight: bold;");
    m_dec5->setStyleSheet("background-color: #8b4500; color: white; font-weight: bold;");
    col2->addWidget(m_inc5);
    col2->addWidget(m_dec5);
    row->addLayout(col2);

    // Boost button
    m_boost = new QPushButton("Boost", this);
    m_boost->setFixedHeight(46);
    m_boost->setAutoRepeat(false);
    m_boost->setCheckable(false);
    row->addWidget(m_boost);

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
    connect(m_zeroBtn, &QPushButton::clicked,  this, &StreamBlock::onZero);

    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    applyVolume();
}

void StreamBlock::setVolume(int vol) {
    int clamped = qBound(0, vol, 100);
    if (m_muted) {
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

void StreamBlock::onInc1() {
    if (m_muted) { m_muted = false; emit muteRequested(m_nodeName, false); }
    m_volume = qMin(100, m_volume + 1);
    applyVolume();
    emit volumeChangeRequested(m_nodeName, m_volume);
}

void StreamBlock::onDec1() {
    if (m_muted) { m_muted = false; emit muteRequested(m_nodeName, false); }
    m_volume = qMax(0, m_volume - 1);
    applyVolume();
    emit volumeChangeRequested(m_nodeName, m_volume);
}

void StreamBlock::onInc5() {
    if (m_muted) { m_muted = false; emit muteRequested(m_nodeName, false); }
    m_volume = qMin(100, m_volume + 5);
    applyVolume();
    emit volumeChangeRequested(m_nodeName, m_volume);
}

void StreamBlock::onDec5() {
    if (m_muted) { m_muted = false; emit muteRequested(m_nodeName, false); }
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

void StreamBlock::onZero() {
    // Momentary — just emit volume 0, do not latch any mute state
    emit volumeChangeRequested(m_nodeName, 0);
}
