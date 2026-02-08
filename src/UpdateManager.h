#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QTemporaryFile;
class QProcess;

class UpdateManager : public QObject
{
    Q_OBJECT
public:
    explicit UpdateManager(QObject *parent = nullptr);
    ~UpdateManager() override;

    // Check for paqet binary updates
    void checkPaqetUpdate();

    // Check for PaqetN GUI updates
    void checkPaqetNUpdate();

    // Download paqet binary
    void downloadPaqet(const QString &version, const QString &downloadUrl);

    // Download PaqetN update
    void downloadPaqetNUpdate(const QString &version, const QString &downloadUrl);

    // Check if paqet binary exists
    bool isPaqetBinaryAvailable(const QString &customPath = QString()) const;

    // Get installed paqet version by running paqet --version
    QString getInstalledPaqetVersion() const;

    // Get current PaqetN version
    QString getPaqetNVersion() const;

    // Cancel ongoing operations
    void cancel();

signals:
    // Paqet update check results
    void paqetUpdateCheckStarted();
    void paqetUpdateCheckFinished(bool updateAvailable, const QString &latestVersion, const QString &downloadUrl);
    void paqetUpdateCheckFailed(const QString &errorMessage);

    // Paqet download progress
    void paqetDownloadStarted();
    void paqetDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void paqetDownloadFinished(const QString &installedPath);
    void paqetDownloadFailed(const QString &errorMessage);

    // PaqetN update check results
    void paqetnUpdateCheckStarted();
    void paqetnUpdateCheckFinished(bool updateAvailable, const QString &latestVersion, const QString &downloadUrl);
    void paqetnUpdateCheckFailed(const QString &errorMessage);

    // PaqetN download progress
    void paqetnDownloadStarted();
    void paqetnDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void paqetnDownloadFinished(); // Triggers self-update and restart
    void paqetnDownloadFailed(const QString &errorMessage);

    // General status
    void statusMessage(const QString &message);
    void installedPaqetVersionChanged(const QString &version);

private slots:
    void onReleaseCheckFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadFinished();

private:
    enum class DownloadType {
        None,
        PaqetBinary,
        PaqetNUpdate
    };

    QString detectPlatform() const;
    QString findAssetUrl(const QByteArray &jsonData, const QString &platform) const;
    bool extractZip(const QString &zipPath, const QString &destDir);
    QString getPaqetInstallDir() const;
    void performSelfUpdate(const QString &newExePath);
    void cleanup();
    
    // Version comparison helper
    // Returns: -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
    int compareVersions(const QString &v1, const QString &v2) const;

    QNetworkAccessManager *m_nam = nullptr;
    QNetworkReply *m_currentReply = nullptr;
    QTemporaryFile *m_downloadFile = nullptr;
    QString m_downloadingVersion;
    DownloadType m_downloadType = DownloadType::None;
    bool m_checkingPaqetN = false;
};
