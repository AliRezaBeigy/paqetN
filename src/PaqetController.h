#pragma once

#include "ConfigListModel.h"
#include "ConfigRepository.h"
#include "SettingsRepository.h"
#include <QObject>
#include <QVariantList>
#include <QVariantMap>

class LogBuffer;
class PaqetRunner;
class LatencyChecker;
class UpdateManager;

class PaqetController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(ConfigListModel* configs READ configs CONSTANT)
    Q_PROPERTY(QString selectedConfigId READ selectedConfigId WRITE setSelectedConfigId NOTIFY selectedConfigIdChanged)
    Q_PROPERTY(QString selectedConfigName READ selectedConfigName NOTIFY selectedConfigIdChanged)
    Q_PROPERTY(bool isRunning READ isRunning NOTIFY isRunningChanged)
    Q_PROPERTY(QString logText READ logText NOTIFY logTextChanged)
    Q_PROPERTY(int latencyMs READ latencyMs NOTIFY latencyMsChanged)
    Q_PROPERTY(bool latencyTesting READ latencyTesting NOTIFY latencyTestingChanged)
    Q_PROPERTY(QVariantMap selectedConfigData READ selectedConfigData NOTIFY selectedConfigIdChanged)
    Q_PROPERTY(bool updateCheckInProgress READ updateCheckInProgress NOTIFY updateCheckInProgressChanged)
    Q_PROPERTY(QString updateStatusMessage READ updateStatusMessage NOTIFY updateStatusMessageChanged)
    Q_PROPERTY(bool paqetDownloadInProgress READ paqetDownloadInProgress NOTIFY paqetDownloadInProgressChanged)
    Q_PROPERTY(int paqetDownloadProgress READ paqetDownloadProgress NOTIFY paqetDownloadProgressChanged)
    Q_PROPERTY(bool paqetnDownloadInProgress READ paqetnDownloadInProgress NOTIFY paqetnDownloadInProgressChanged)
    Q_PROPERTY(int paqetnDownloadProgress READ paqetnDownloadProgress NOTIFY paqetnDownloadProgressChanged)
    Q_PROPERTY(QString installedPaqetVersion READ installedPaqetVersion NOTIFY installedPaqetVersionChanged)
public:
    explicit PaqetController(QObject *parent = nullptr);
    ~PaqetController() override;

    ConfigListModel *configs() const { return m_configList; }
    QString selectedConfigId() const { return m_selectedConfigId; }
    void setSelectedConfigId(const QString &id);
    QString selectedConfigName() const;
    bool isRunning() const;
    QString logText() const;
    int latencyMs() const { return m_latencyMs; }
    bool latencyTesting() const { return m_latencyTesting; }
    QVariantMap selectedConfigData() const;
    bool updateCheckInProgress() const { return m_updateCheckInProgress; }
    QString updateStatusMessage() const { return m_updateStatusMessage; }
    bool paqetDownloadInProgress() const { return m_paqetDownloadInProgress; }
    int paqetDownloadProgress() const { return m_paqetDownloadProgress; }
    bool paqetnDownloadInProgress() const { return m_paqetnDownloadInProgress; }
    int paqetnDownloadProgress() const { return m_paqetnDownloadProgress; }
    QString installedPaqetVersion() const;

    Q_INVOKABLE QVariantMap getConfigForEdit(const QString &id);
    Q_INVOKABLE QVariantList getGroups();
    Q_INVOKABLE void saveConfig(const QVariantMap &config);
    Q_INVOKABLE void deleteConfig(const QString &id);
    Q_INVOKABLE void renameGroup(const QString &oldName, const QString &newName);
    Q_INVOKABLE bool addConfigFromImport(const QString &text);
    Q_INVOKABLE QString exportPaqetUri(const QString &id);
    Q_INVOKABLE QString exportYaml(const QString &id);
    Q_INVOKABLE void connectToSelected();
    Q_INVOKABLE void disconnect();
    Q_INVOKABLE void testLatency();
    Q_INVOKABLE void clearLog();
    Q_INVOKABLE void copyToClipboard(const QString &text);
    Q_INVOKABLE QString getClipboardText() const;
    Q_INVOKABLE QString readFile(const QString &path) const;
    Q_INVOKABLE bool writeFile(const QString &path, const QString &content) const;
    Q_INVOKABLE QString getTheme() const;
    Q_INVOKABLE void setTheme(const QString &theme);
    Q_INVOKABLE int getSocksPort() const;
    Q_INVOKABLE void setSocksPort(int port);
    Q_INVOKABLE QString getConnectionCheckUrl() const;
    Q_INVOKABLE void setConnectionCheckUrl(const QString &url);
    Q_INVOKABLE int getConnectionCheckTimeoutSeconds() const;
    Q_INVOKABLE void setConnectionCheckTimeoutSeconds(int seconds);
    Q_INVOKABLE bool getShowLatencyInUi() const;
    Q_INVOKABLE void setShowLatencyInUi(bool show);
    Q_INVOKABLE QString getLogLevel() const;
    Q_INVOKABLE void setLogLevel(const QString &level);
    Q_INVOKABLE QString getPaqetBinaryPath() const;
    Q_INVOKABLE void setPaqetBinaryPath(const QString &path);
    Q_INVOKABLE QStringList getLogLevels() const;

    // Update Manager methods
    Q_INVOKABLE QString getPaqetNVersion() const;
    Q_INVOKABLE QString getInstalledPaqetVersion();
    Q_INVOKABLE void checkPaqetUpdate();
    Q_INVOKABLE void checkPaqetNUpdate();
    Q_INVOKABLE void downloadPaqet(const QString &version, const QString &url);
    Q_INVOKABLE void downloadPaqetNUpdate(const QString &version, const QString &url);
    Q_INVOKABLE void cancelUpdate();
    Q_INVOKABLE bool isPaqetBinaryAvailable() const;
    Q_INVOKABLE void autoDownloadPaqetIfMissing();
    Q_INVOKABLE bool getAutoDownloadPaqet() const;
    Q_INVOKABLE void setAutoDownloadPaqet(bool enabled);
    Q_INVOKABLE bool getAutoCheckUpdates() const;
    Q_INVOKABLE void setAutoCheckUpdates(bool enabled);
    Q_INVOKABLE bool getAutoUpdatePaqetN() const;
    Q_INVOKABLE void setAutoUpdatePaqetN(bool enabled);

    // Network detection
    Q_INVOKABLE QVariantList detectNetworkAdapters();
    Q_INVOKABLE QVariantMap getDefaultNetworkAdapter();

signals:
    void selectedConfigIdChanged();
    void isRunningChanged();
    void logTextChanged();
    void latencyMsChanged();
    void latencyTestingChanged();
    void configsChanged();
    void updateCheckInProgressChanged();
    void updateStatusMessageChanged();
    void paqetDownloadInProgressChanged();
    void paqetDownloadProgressChanged();
    void paqetnDownloadInProgressChanged();
    void paqetnDownloadProgressChanged();
    void installedPaqetVersionChanged();
    void paqetUpdateAvailable(const QString &version, const QString &url);
    void paqetNUpdateAvailable(const QString &version, const QString &url);
    void paqetDownloadComplete(const QString &path);
    void paqetnDownloadComplete();
    void paqetBinaryMissing();
    void paqetBinaryMissingPrompt();

private slots:
    void onPaqetUpdateCheckFinished(bool available, const QString &version, const QString &url);
    void onPaqetNUpdateCheckFinished(bool available, const QString &version, const QString &url);
    void onPaqetDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onPaqetNDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onPaqetDownloadFinished(const QString &path);
    void onPaqetNDownloadFinished();

private:
    void reloadConfigList();
    PaqetConfig selectedConfig() const;

    ConfigRepository *m_repo = nullptr;
    SettingsRepository *m_settings = nullptr;
    ConfigListModel *m_configList = nullptr;
    LogBuffer *m_logBuffer = nullptr;
    PaqetRunner *m_runner = nullptr;
    LatencyChecker *m_latencyChecker = nullptr;
    UpdateManager *m_updateManager = nullptr;
    QString m_selectedConfigId;
    QString m_connectedConfigId;
    int m_latencyMs = -1;
    bool m_latencyTesting = false;
    bool m_updateCheckInProgress = false;
    QString m_updateStatusMessage;
    bool m_paqetDownloadInProgress = false;
    int m_paqetDownloadProgress = 0;
    bool m_paqetnDownloadInProgress = false;
    int m_paqetnDownloadProgress = 0;
    bool m_autoDownloadMode = false; // Track if current check is for auto-download
};
