#pragma once

#include <QObject>
#include <QString>

class QSettings;

class SettingsRepository : public QObject
{
    Q_OBJECT
public:
    explicit SettingsRepository(QObject *parent = nullptr);

    QString theme() const;
    void setTheme(const QString &theme);

    int socksPort() const;
    void setSocksPort(int port);

    QString connectionCheckUrl() const;
    void setConnectionCheckUrl(const QString &url);

    int connectionCheckTimeoutSeconds() const;
    void setConnectionCheckTimeoutSeconds(int seconds);

    bool showLatencyInUi() const;
    void setShowLatencyInUi(bool show);

    QString logLevel() const;
    void setLogLevel(const QString &level);

    QString paqetBinaryPath() const;
    void setPaqetBinaryPath(const QString &path);

    bool autoDownloadPaqet() const;
    void setAutoDownloadPaqet(bool enabled);

    bool autoCheckUpdates() const;
    void setAutoCheckUpdates(bool enabled);

    bool autoUpdatePaqetN() const;
    void setAutoUpdatePaqetN(bool enabled);

    QString proxyMode() const;           // "none" | "system" | "tun"
    void setProxyMode(const QString &mode);

    QString tunBinaryPath() const;
    void setTunBinaryPath(const QString &path);

    bool startOnBoot() const;
    void setStartOnBoot(bool enabled);

    bool autoHideOnStartup() const;
    void setAutoHideOnStartup(bool enabled);

    bool closeToTray() const;
    void setCloseToTray(bool enabled);

    static const QStringList &logLevels();
    static const QStringList &proxyModes();
    static constexpr int defaultSocksPort = 1284;
    static constexpr const char *defaultConnectionCheckUrl = "https://www.gstatic.com/generate_204";
    static constexpr int defaultConnectionCheckTimeoutSeconds = 10;
    static constexpr int minConnectionCheckTimeout = 3;
    static constexpr int maxConnectionCheckTimeout = 60;

signals:
    void themeChanged();
    void socksPortChanged();
    void connectionCheckUrlChanged();
    void connectionCheckTimeoutSecondsChanged();
    void showLatencyInUiChanged();
    void logLevelChanged();
    void paqetBinaryPathChanged();
    void autoDownloadPaqetChanged();
    void autoCheckUpdatesChanged();
    void autoUpdatePaqetNChanged();
    void proxyModeChanged();
    void tunBinaryPathChanged();
    void startOnBootChanged();
    void autoHideOnStartupChanged();
    void closeToTrayChanged();

private:
    QSettings *settings() const;
    mutable QSettings *m_settings = nullptr;
};
