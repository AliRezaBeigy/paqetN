#pragma once

#include <QObject>
#include <QString>

class QNetworkAccessManager;
class QNetworkReply;
class QTemporaryFile;
class LogBuffer;

class TunManager;

class TunAssetsManager : public QObject
{
    Q_OBJECT
public:
    explicit TunAssetsManager(LogBuffer *logBuffer, TunManager *tunManager, QObject *parent = nullptr);
    ~TunAssetsManager() override;

    // Check if hev-socks5-tunnel (and on Windows wintun.dll) are available
    bool isTunAssetsAvailable() const;

    // Download hev-socks5-tunnel and (on Windows) wintun.dll to app dir
    void downloadTunAssets();

    // Whether a download is in progress
    bool downloadInProgress() const { return m_downloadInProgress; }

signals:
    void tunAssetsMissingPrompt();
    void tunAssetsDownloadStarted();
    void tunAssetsDownloadProgress(int percent);
    void tunAssetsDownloadFinished();
    void tunAssetsDownloadFailed(const QString &error);

private slots:
    void onHevReleaseCheckFinished();
    void onWintunReleaseCheckFinished();
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onHevDownloadFinished();
    void onWintunDownloadFinished();

private:
    enum class Step { None, HevCheck, HevDownload, WintunCheck, WintunDownload };
    QString detectPlatform() const;
    QString findHevAssetUrl(const QByteArray &jsonData) const;
    QString findWintunAssetUrl(const QByteArray &jsonData) const;
    bool extractZip(const QString &zipPath, const QString &destDir);
    QString getInstallDir() const;
    void startHevDownload(const QString &url);
    void startWintunDownload(const QString &url);
    void nextStep();
    void cleanup();

    LogBuffer *m_logBuffer = nullptr;
    TunManager *m_tunManager = nullptr;
    QNetworkAccessManager *m_nam = nullptr;
    QNetworkReply *m_currentReply = nullptr;
    QTemporaryFile *m_downloadFile = nullptr;
    Step m_step = Step::None;
    bool m_downloadInProgress = false;
    qint64 m_downloadTotal = 0;
};
