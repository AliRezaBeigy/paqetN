#include "SettingsRepository.h"
#include <QSettings>
#include <QCoreApplication>

static const QStringList logLevelList = {
    QStringLiteral("none"), QStringLiteral("debug"), QStringLiteral("info"),
    QStringLiteral("warn"), QStringLiteral("error"), QStringLiteral("fatal")
};

const QStringList &SettingsRepository::logLevels() { return logLevelList; }

QSettings *SettingsRepository::settings() const {
    if (!m_settings)
        m_settings = new QSettings(QSettings::UserScope,
                                  QCoreApplication::organizationName(),
                                  QCoreApplication::applicationName(),
                                  const_cast<SettingsRepository *>(this));
    return m_settings;
}

SettingsRepository::SettingsRepository(QObject *parent) : QObject(parent) {}

QString SettingsRepository::theme() const {
    return settings()->value(QStringLiteral("theme"), QStringLiteral("system")).toString();
}

void SettingsRepository::setTheme(const QString &theme) {
    if (this->theme() == theme) return;
    settings()->setValue(QStringLiteral("theme"), theme);
    emit themeChanged();
}

int SettingsRepository::socksPort() const {
    return settings()->value(QStringLiteral("socksPort"), defaultSocksPort).toInt();
}

void SettingsRepository::setSocksPort(int port) {
    port = qBound(1, port, 65535);
    if (socksPort() == port) return;
    settings()->setValue(QStringLiteral("socksPort"), port);
    emit socksPortChanged();
}

QString SettingsRepository::connectionCheckUrl() const {
    return settings()->value(QStringLiteral("connectionCheckUrl"), QString::fromUtf8(defaultConnectionCheckUrl)).toString();
}

void SettingsRepository::setConnectionCheckUrl(const QString &url) {
    QString v = url.trimmed();
    if (v.isEmpty()) v = QString::fromUtf8(defaultConnectionCheckUrl);
    if (connectionCheckUrl() == v) return;
    settings()->setValue(QStringLiteral("connectionCheckUrl"), v);
    emit connectionCheckUrlChanged();
}

int SettingsRepository::connectionCheckTimeoutSeconds() const {
    return settings()->value(QStringLiteral("connectionCheckTimeoutSeconds"), defaultConnectionCheckTimeoutSeconds).toInt();
}

void SettingsRepository::setConnectionCheckTimeoutSeconds(int seconds) {
    seconds = qBound(minConnectionCheckTimeout, seconds, maxConnectionCheckTimeout);
    if (connectionCheckTimeoutSeconds() == seconds) return;
    settings()->setValue(QStringLiteral("connectionCheckTimeoutSeconds"), seconds);
    emit connectionCheckTimeoutSecondsChanged();
}

bool SettingsRepository::showLatencyInUi() const {
    return settings()->value(QStringLiteral("showLatencyInUi"), true).toBool();
}

void SettingsRepository::setShowLatencyInUi(bool show) {
    if (showLatencyInUi() == show) return;
    settings()->setValue(QStringLiteral("showLatencyInUi"), show);
    emit showLatencyInUiChanged();
}

QString SettingsRepository::logLevel() const {
    return settings()->value(QStringLiteral("logLevel"), QStringLiteral("fatal")).toString();
}

void SettingsRepository::setLogLevel(const QString &level) {
    QString v = logLevelList.contains(level) ? level : QStringLiteral("fatal");
    if (this->logLevel() == v) return;
    settings()->setValue(QStringLiteral("logLevel"), v);
    emit logLevelChanged();
}

QString SettingsRepository::paqetBinaryPath() const {
    return settings()->value(QStringLiteral("paqetBinaryPath")).toString();
}

void SettingsRepository::setPaqetBinaryPath(const QString &path) {
    if (paqetBinaryPath() == path) return;
    settings()->setValue(QStringLiteral("paqetBinaryPath"), path);
    emit paqetBinaryPathChanged();
}

bool SettingsRepository::autoDownloadPaqet() const {
    return settings()->value(QStringLiteral("autoDownloadPaqet"), true).toBool();
}

void SettingsRepository::setAutoDownloadPaqet(bool enabled) {
    if (autoDownloadPaqet() == enabled) return;
    settings()->setValue(QStringLiteral("autoDownloadPaqet"), enabled);
    emit autoDownloadPaqetChanged();
}

bool SettingsRepository::autoCheckUpdates() const {
    return settings()->value(QStringLiteral("autoCheckUpdates"), true).toBool();
}

void SettingsRepository::setAutoCheckUpdates(bool enabled) {
    if (autoCheckUpdates() == enabled) return;
    settings()->setValue(QStringLiteral("autoCheckUpdates"), enabled);
    emit autoCheckUpdatesChanged();
}

bool SettingsRepository::autoUpdatePaqetN() const {
    return settings()->value(QStringLiteral("autoUpdatePaqetN"), false).toBool();
}

void SettingsRepository::setAutoUpdatePaqetN(bool enabled) {
    if (autoUpdatePaqetN() == enabled) return;
    settings()->setValue(QStringLiteral("autoUpdatePaqetN"), enabled);
    emit autoUpdatePaqetNChanged();
}
