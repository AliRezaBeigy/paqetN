#include "ConfigRepository.h"
#include <QSettings>
#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QSaveFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QUuid>
#include <algorithm>

ConfigRepository::ConfigRepository(QObject *parent) : QObject(parent) {
    m_lastSelectedId = QSettings(QSettings::UserScope, QStringLiteral("paqetN"), QStringLiteral("paqetN"))
        .value(QStringLiteral("lastSelectedConfigId")).toString();
}

QString ConfigRepository::configFilePath() const {
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    return dir + QLatin1String("/configs.json");
}

bool ConfigRepository::load(QList<PaqetConfig> *out) const {
    QFile f(configFilePath());
    if (!f.open(QIODevice::ReadOnly))
        return false;
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(f.readAll(), &err);
    f.close();
    if (err.error != QJsonParseError::NoError || !doc.isArray())
        return false;
    out->clear();
    for (const QJsonValue &v : doc.array()) {
        if (!v.isObject()) continue;
        PaqetConfig c = PaqetConfig::fromVariantMap(v.toObject().toVariantMap());
        out->append(c);
    }
    return true;
}

bool ConfigRepository::save(const QList<PaqetConfig> &list) {
    QJsonArray arr;
    for (const PaqetConfig &c : list)
        arr.append(QJsonObject::fromVariantMap(c.toVariantMap()));
    QSaveFile f(configFilePath());
    if (!f.open(QIODevice::WriteOnly))
        return false;
    f.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    return f.commit();
}

QList<PaqetConfig> ConfigRepository::configs() const {
    QList<PaqetConfig> list;
    load(&list);
    return list;
}

QString ConfigRepository::lastSelectedId() const {
    return m_lastSelectedId;
}

void ConfigRepository::setLastSelectedId(const QString &id) {
    if (m_lastSelectedId == id) return;
    m_lastSelectedId = id;
    QSettings(QSettings::UserScope, QStringLiteral("paqetN"), QStringLiteral("paqetN"))
        .setValue(QStringLiteral("lastSelectedConfigId"), id);
}

QString ConfigRepository::add(const PaqetConfig &config) {
    QList<PaqetConfig> list;
    load(&list);
    PaqetConfig c = config.withDefaults();
    c.id = config.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : config.id;
    list.append(c);
    if (save(list)) {
        emit configsChanged();
        return c.id;
    }
    return QString();
}

void ConfigRepository::update(const PaqetConfig &config) {
    QList<PaqetConfig> list;
    if (!load(&list)) return;
    for (int i = 0; i < list.size(); ++i) {
        if (list[i].id == config.id) {
            // Preserve empty flag arrays when updating - don't force defaults
            // Save original flag state before withDefaults() modifies them
            bool localFlagWasEmpty = config.localFlag.isEmpty();
            bool remoteFlagWasEmpty = config.remoteFlag.isEmpty();
            PaqetConfig updated = config.withDefaults();
            updated.id = config.id;
            // Restore empty arrays if they were explicitly set to empty
            if (localFlagWasEmpty) {
                updated.localFlag.clear();
            }
            if (remoteFlagWasEmpty) {
                updated.remoteFlag.clear();
            }
            list[i] = updated;
            if (save(list)) emit configsChanged();
            return;
        }
    }
}

void ConfigRepository::remove(const QString &id) {
    QList<PaqetConfig> list;
    if (!load(&list)) return;
    auto it = std::remove_if(list.begin(), list.end(), [&id](const PaqetConfig &c) { return c.id == id; });
    if (it != list.end()) {
        list.erase(it, list.end());
        if (save(list)) {
            if (m_lastSelectedId == id) setLastSelectedId(QString());
            emit configsChanged();
        }
    }
}

PaqetConfig ConfigRepository::getById(const QString &id) const {
    QList<PaqetConfig> list;
    load(&list);
    for (const PaqetConfig &c : list)
        if (c.id == id) return c;
    return PaqetConfig();
}

void ConfigRepository::renameGroup(const QString &oldName, const QString &newName) {
    if (oldName.isEmpty() || newName.isEmpty() || oldName == newName) return;
    QList<PaqetConfig> list;
    if (!load(&list)) return;
    bool changed = false;
    for (PaqetConfig &c : list) {
        if (c.group == oldName) {
            c.group = newName;
            changed = true;
        }
    }
    if (changed && save(list)) {
        emit configsChanged();
    }
}
