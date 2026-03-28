#include "Config.h"

Config& Config::instance() {
    static Config inst;
    return inst;
}

void Config::load() {
    m_settings.beginGroup("streams");
    const QStringList ids = m_settings.childGroups();
    for (const QString& id : ids) {
        m_settings.beginGroup(id);
        StreamConfig sc;
        sc.id          = id;
        sc.displayName = m_settings.value("displayName", id).toString();
        sc.selected    = m_settings.value("selected", false).toBool();
        sc.color       = QColor(m_settings.value("color", "#777777").toString());
        m_streams[id]  = sc;
        m_settings.endGroup();
    }
    m_settings.endGroup();
}

void Config::save() {
    m_settings.beginGroup("streams");
    for (auto it = m_streams.cbegin(); it != m_streams.cend(); ++it) {
        const StreamConfig& sc = it.value();
        m_settings.beginGroup(sc.id);
        m_settings.setValue("displayName", sc.displayName);
        m_settings.setValue("selected",    sc.selected);
        m_settings.setValue("color",       sc.color.name());
        m_settings.endGroup();
    }
    m_settings.endGroup();
    m_settings.sync();
}

StreamConfig& Config::streamConfig(const QString& id) {
    if (!m_streams.contains(id)) {
        StreamConfig sc;
        sc.id          = id;
        sc.displayName = id;
        sc.selected    = false;
        sc.color       = QColor("#777777");
        m_streams[id]  = sc;
    }
    return m_streams[id];
}
