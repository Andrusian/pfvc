#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QString>
#include <QPainter>

// Dual-channel VU bar — L on top, R below, with 10% graduation marks.
class VuBar : public QWidget {
    Q_OBJECT
public:
    explicit VuBar(QWidget* parent = nullptr) : QWidget(parent) {
        setFixedHeight(22);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setLevels(int levelL, int levelR) {
        m_levelL = qBound(0, levelL, 100);
        m_levelR = qBound(0, levelR, 100);
        update();
    }

    void setLevel(int level) { setLevels(level, level); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        const int w      = rect().width();
        const int labelW = 10;
        const int barX   = labelW;
        const int barW   = w - labelW;
        const int barH   = 9;
        const int gap    = 4;
        const int topY   = 0;
        const int botY   = barH + gap;

        p.setPen(QColor("#aaaaaa"));
        QFont f = p.font();
        f.setPixelSize(8);
        p.setFont(f);
        p.drawText(0, topY, labelW - 1, barH, Qt::AlignRight | Qt::AlignVCenter, "L");
        p.drawText(0, botY, labelW - 1, barH, Qt::AlignRight | Qt::AlignVCenter, "R");

        auto drawBar = [&](int y, int level) {
            p.fillRect(barX, y, barW, barH, QColor("#222222"));
            int fillW = (barW * level) / 100;
            if (fillW > 0)
                p.fillRect(barX, y, fillW, barH, QColor("#00ff00"));
            p.setRenderHint(QPainter::Antialiasing, false);
            for (int pct = 10; pct < 100; pct += 10) {
                int x = barX + (barW * pct) / 100;
                p.setPen(QPen(QColor("#777777"), pct == 50 ? 2 : 1));
                p.drawLine(x, y, x, y + barH - 1);
            }
            p.setPen(QPen(QColor("#000000"), 1));
            p.drawRect(barX, y, barW - 1, barH - 1);
        };

        drawBar(topY, m_levelL);
        drawBar(botY, m_levelR);
    }

private:
    int m_levelL = 0;
    int m_levelR = 0;
};

class StreamBlock : public QWidget {
    Q_OBJECT

public:
    // nodeName  — stable node.name string, used as config key and signal id
    // pwId      — current session numeric PipeWire id, used for wpctl calls
    // displayName — human-readable label shown in the UI
    explicit StreamBlock(const QString& nodeName,
                         uint32_t       pwId,
                         const QString& displayName,
                         QWidget*       parent = nullptr);

    // The stable key (node.name) — used in all signals and map lookups
    QString  nodeName() const { return m_nodeName; }

    // The current PipeWire numeric id — may change on hot-plug
    uint32_t pwId()     const { return m_pwId; }
    void     setPwId(uint32_t id) { m_pwId = id; }

    void setVolume(int vol);
    void setMuted(bool muted);
    int  volume()  const { return m_volume; }
    bool isMuted() const { return m_muted; }

    void setBlockColor(const QColor& color);
    void setVuLevel(int levelL, int levelR);

signals:
    // Both signals carry the stable nodeName as the id string
    void volumeChangeRequested(const QString& nodeName, int newVolume);
    void muteRequested(const QString& nodeName, bool mute);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void focusInEvent(QFocusEvent* event) override;
    void focusOutEvent(QFocusEvent* event) override;

private slots:
    void onInc1();
    void onDec1();
    void onInc5();
    void onDec5();
    void onBoostPressed();
    void onBoostReleased();
    void onMute();

private:
    void applyVolume();
    void buildUi(const QString& displayName);

    QString      m_nodeName;            // stable key
    uint32_t     m_pwId       = 0;      // session-local PW id
    int          m_volume     = 100;
    int          m_preBoost   = 100;
    bool         m_muted      = false;
    int          m_preMute    = 100;
    QColor       m_blockColor = QColor("#777777");
    bool         m_focused    = false;

    QLabel*      m_nameLabel  = nullptr;
    QLabel*      m_volDisplay = nullptr;
    QPushButton* m_inc1       = nullptr;
    QPushButton* m_dec1       = nullptr;
    QPushButton* m_inc5       = nullptr;
    QPushButton* m_dec5       = nullptr;
    QPushButton* m_boost      = nullptr;
    QPushButton* m_muteBtn    = nullptr;
    VuBar*       m_vuBar      = nullptr;
};
