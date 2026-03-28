#include "SettingsDialog.h"
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QScrollArea>
#include <QFrame>

// Set this to true if you need to debug or manage offline ephemeral streams
static const bool SHOW_UNAVAILABLE_STREAMS = false;

SettingsDialog::SettingsDialog(PipeWireManager* pw, QWidget* parent)
    : QDialog(parent), m_pw(pw)
{
    setWindowTitle("Settings — Output Devices");
    setMinimumWidth(420);
    setModal(true);

    auto* outer = new QVBoxLayout(this);
    outer->setSpacing(8);

    auto* header = new QLabel(
        "Select which output devices are shown on the main screen.\n"
        "New devices are hidden until enabled here.", this);
    header->setWordWrap(true);
    header->setStyleSheet("color: gray; font-size: 11px;");
    outer->addWidget(header);

    auto* scroll     = new QScrollArea(this);
    auto* content    = new QWidget();
    auto* listLayout = new QVBoxLayout(content);
    listLayout->setSpacing(6);
    listLayout->setContentsMargins(4, 4, 4, 4);

    // Merge live sinks (keyed by nodeName)
    QMap<QString, PwNodeInfo> sinkMap;
    for (const PwNodeInfo& info : pw->knownSinks())
        sinkMap[info.nodeName] = info;

    // Condition to include saved sinks that are not currently live
    if (SHOW_UNAVAILABLE_STREAMS) {
        for (auto it = Config::instance().streams().cbegin();
             it != Config::instance().streams().cend(); ++it) {
            if (!sinkMap.contains(it->id)) {
                PwNodeInfo ghost;
                ghost.id         = 0;
                ghost.nodeName   = it->id;           // id IS the nodeName in Config
                ghost.name       = it->displayName + " (unavailable)";
                ghost.mediaClass = "Audio/Sink";
                sinkMap[it->id]  = ghost;
            }
        }
    }

    if (sinkMap.isEmpty()) {
        listLayout->addWidget(new QLabel("No output devices found.", content));
    }

    
    for (auto it = sinkMap.cbegin(); it != sinkMap.cend(); ++it) {
        const PwNodeInfo& info = it.value();
        const QString& nodeKey = info.nodeName;   // stable key

        auto& sc = Config::instance().streamConfig(nodeKey);
        if (sc.displayName == nodeKey)            // first time — use human name
            sc.displayName = info.name;

        auto* row    = new QWidget(content);
        auto* rowLay = new QHBoxLayout(row);
        rowLay->setContentsMargins(0, 0, 0, 0);

        auto* cb = new QCheckBox(info.name, row);
        cb->setChecked(sc.selected);
        if (info.name.endsWith("(unavailable)"))
            cb->setStyleSheet("color: gray;");

        auto* colorBtn = new QPushButton(row);
        colorBtn->setFixedSize(28, 22);
        colorBtn->setToolTip("Pick color for this device");

        SinkRow sr;
        sr.id       = nodeKey;
        sr.checkbox = cb;
        sr.colorBtn = colorBtn;
        sr.color    = sc.color;

        colorBtn->setStyleSheet(
            QString("background-color: %1; border: 1px solid #555;")
                .arg(sr.color.name()));

        connect(colorBtn, &QPushButton::clicked, this, [this, nodeKey]() {
            onColorPick(nodeKey);
        });

        rowLay->addWidget(cb, 1);
        rowLay->addWidget(colorBtn);
        listLayout->addWidget(row);

        auto* line = new QFrame(content);
        line->setFrameShape(QFrame::HLine);
        line->setStyleSheet("color: #444;");
        listLayout->addWidget(line);

        m_rows.append(sr);
    }

    listLayout->addStretch();
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumHeight(200);
    outer->addWidget(scroll);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &SettingsDialog::onAccept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    outer->addWidget(buttons);
}

void SettingsDialog::onAccept() {
    for (const SinkRow& sr : m_rows) {
        auto& sc    = Config::instance().streamConfig(sr.id);
        sc.selected = sr.checkbox->isChecked();
        sc.color    = sr.color;
    }
    accept();
}

void SettingsDialog::onColorPick(const QString& id) {
    for (SinkRow& sr : m_rows) {
        if (sr.id != id) continue;
        QColor picked = QColorDialog::getColor(sr.color, this,
                                               "Choose device color");
        if (picked.isValid()) {
            sr.color = picked;
            sr.colorBtn->setStyleSheet(
                QString("background-color: %1; border: 1px solid #555;")
                    .arg(picked.name()));
        }
        break;
    }
}
