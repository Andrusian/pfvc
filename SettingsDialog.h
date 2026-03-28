#pragma once
#include <QDialog>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QCheckBox>
#include <QPushButton>
#include <QLabel>
#include <QMap>
#include "PipeWireManager.h"
#include "Config.h"

class SettingsDialog : public QDialog {
    Q_OBJECT

public:
    explicit SettingsDialog(PipeWireManager* pw, QWidget* parent = nullptr);

private slots:
    void onAccept();
    void onColorPick(const QString& id);

private:
    struct SinkRow {
        QString      id;
        QCheckBox*   checkbox  = nullptr;
        QPushButton* colorBtn  = nullptr;
        QColor       color;
    };

    PipeWireManager*    m_pw;
    QList<SinkRow>      m_rows;
};
