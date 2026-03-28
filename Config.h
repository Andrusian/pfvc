#pragma once
#include <QSettings>
#include <QString>
#include <QColor>
#include <QMap>

// Describes a stream as remembered across sessions.
struct StreamConfig {
    QString  id;           // PipeWire node id (as string) or stable name
    QString  displayName;
    bool     selected  = false;
    QColor   color     = QColor("#777777");
};

class Config {
public:
    static Config& instance();

    void load();
    void save();

    // Returns config for a stream id, creating a default entry if unseen.
    StreamConfig& streamConfig(const QString& id);

    // All known stream configs (persisted).
    QMap<QString, StreamConfig>& streams() { return m_streams; }

private:
    Config() = default;
    QSettings               m_settings{"pfvc", "pfvc"};
    QMap<QString, StreamConfig> m_streams;
};
