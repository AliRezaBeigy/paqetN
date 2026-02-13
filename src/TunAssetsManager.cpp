#include "TunAssetsManager.h"
#include "TunManager.h"
#include "LogBuffer.h"
#include "ZipExtractor.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryFile>
#include <QStandardPaths>
#include <QDirIterator>
#include <QDebug>

#ifdef Q_OS_WIN
#include <QProcess>
#endif

TunAssetsManager::TunAssetsManager(LogBuffer *logBuffer, TunManager *tunManager, QObject *parent)
    : QObject(parent), m_logBuffer(logBuffer), m_tunManager(tunManager)
{
    m_nam = new QNetworkAccessManager(this);
}

TunAssetsManager::~TunAssetsManager()
{
    cleanup();
}

bool TunAssetsManager::isTunAssetsAvailable() const
{
    if (!m_tunManager) return false;
    QString hevPath = m_tunManager->resolveTunBinary();
    QFileInfo hevInfo(hevPath);
    if (!hevInfo.exists() || !hevInfo.isFile()) return false;
#ifdef Q_OS_WIN
    // On Windows, hev-socks5-tunnel needs wintun.dll and msys-2.0.dll in cores folder
    QString coresDir = QCoreApplication::applicationDirPath() + QDir::separator() + QStringLiteral("cores");
    if (!QFileInfo::exists(coresDir + QDir::separator() + QStringLiteral("wintun.dll")))
        return false;
    if (!QFileInfo::exists(coresDir + QDir::separator() + QStringLiteral("msys-2.0.dll")))
        return false;
#endif
    return true;
}

QString TunAssetsManager::detectPlatform() const
{
#ifdef Q_OS_WIN
    return QStringLiteral("windows-amd64");
#elif defined(Q_OS_MACOS)
#if defined(Q_PROCESSOR_ARM)
    return QStringLiteral("darwin-arm64");
#else
    return QStringLiteral("darwin-x86_64");
#endif
#elif defined(Q_OS_LINUX)
    return QStringLiteral("linux-amd64");
#else
    return QString();
#endif
}

QString TunAssetsManager::findHevAssetUrl(const QByteArray &jsonData) const
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isObject()) return QString();
    QJsonArray assets = doc.object()[QStringLiteral("assets")].toArray();
    QString platform = detectPlatform();
    QString pattern = QStringLiteral("hev-socks5-tunnel-%1").arg(platform);
    for (const QJsonValue &v : assets) {
        QJsonObject asset = v.toObject();
        QString name = asset[QStringLiteral("name")].toString();
        if (name.contains(pattern, Qt::CaseInsensitive))
            return asset[QStringLiteral("browser_download_url")].toString();
    }
    return QString();
}

QString TunAssetsManager::findWintunAssetUrl(const QByteArray &jsonData) const
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isArray()) return QString();
    QJsonArray releases = doc.array();
    for (const QJsonValue &v : releases) {
        if (!v.isObject()) continue;
        QJsonObject rel = v.toObject();
        QJsonArray assets = rel[QStringLiteral("assets")].toArray();
        for (const QJsonValue &a : assets) {
            QJsonObject asset = a.toObject();
            QString name = asset[QStringLiteral("name")].toString();
            // e.g. wintun-0.14.1.zip or wintun-amd64-0.14.1.zip
            if (name.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive) && name.contains(QStringLiteral("wintun"), Qt::CaseInsensitive))
                return asset[QStringLiteral("browser_download_url")].toString();
        }
    }
    return QString();
}

bool TunAssetsManager::extractZip(const QString &zipPath, const QString &destDir)
{
    QString err;
    return ZipExtractor::extractFile(zipPath, destDir, err);
}

QString TunAssetsManager::getInstallDir() const
{
    return QCoreApplication::applicationDirPath() + QDir::separator() + QStringLiteral("cores");
}

void TunAssetsManager::downloadTunAssets()
{
    if (m_downloadInProgress) return;
    m_downloadInProgress = true;
    emit tunAssetsDownloadStarted();

#ifdef Q_OS_WIN
    // Windows: Direct download from known URL (zip contains hev-socks5-tunnel.exe, wintun.dll, msys-2.0.dll)
    m_step = Step::HevDownload;
    startHevDownload(QStringLiteral("https://github.com/heiher/hev-socks5-tunnel/releases/download/2.14.4/hev-socks5-tunnel-win64.zip"));
#else
    // Other platforms: Check GitHub API for latest release
    m_step = Step::HevCheck;
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TunAssets] Checking hev-socks5-tunnel releases..."));
    QNetworkRequest req(QUrl(QStringLiteral("https://api.github.com/repos/heiher/hev-socks5-tunnel/releases/latest")));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("PaqetN/1.0"));
    req.setTransferTimeout(6000);
    m_currentReply = m_nam->get(req);
    connect(m_currentReply, &QNetworkReply::finished, this, &TunAssetsManager::onHevReleaseCheckFinished);
#endif
}

void TunAssetsManager::onHevReleaseCheckFinished()
{
    if (!m_currentReply || m_step != Step::HevCheck) return;
    if (m_currentReply->error() != QNetworkReply::NoError) {
        QString err = m_currentReply->errorString();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        m_downloadInProgress = false;
        emit tunAssetsDownloadFailed(tr("Failed to fetch hev-socks5-tunnel releases: %1").arg(err));
        return;
    }
    QByteArray data = m_currentReply->readAll();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    QString url = findHevAssetUrl(data);
    if (url.isEmpty()) {
        m_downloadInProgress = false;
        emit tunAssetsDownloadFailed(tr("No hev-socks5-tunnel binary found for your platform."));
        return;
    }
    startHevDownload(url);
}

void TunAssetsManager::startHevDownload(const QString &url)
{
    m_step = Step::HevDownload;
    m_downloadFile = new QTemporaryFile(this);
    if (!m_downloadFile->open()) {
        cleanup();
        emit tunAssetsDownloadFailed(tr("Failed to create temporary file"));
        return;
    }
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TunAssets] Downloading hev-socks5-tunnel..."));
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("PaqetN/1.0"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(6000);
    m_currentReply = m_nam->get(req);
    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &TunAssetsManager::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished, this, &TunAssetsManager::onHevDownloadFinished);
}

void TunAssetsManager::onHevDownloadFinished()
{
    if (!m_currentReply || !m_downloadFile || m_step != Step::HevDownload) return;
    if (m_currentReply->error() != QNetworkReply::NoError) {
        QString err = m_currentReply->errorString();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        cleanup();
        emit tunAssetsDownloadFailed(tr("Download failed: %1").arg(err));
        return;
    }
    m_downloadFile->write(m_currentReply->readAll());
    m_downloadFile->flush();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    QString zipPath = m_downloadFile->fileName();
    QString installDir = getInstallDir();
    QDir().mkpath(installDir);

#ifdef Q_OS_WIN
    // Windows: Extract zip containing hev-socks5-tunnel folder with exe and dlls
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QLatin1String("/paqetN_hev");
    QDir().mkpath(tempDir);
    if (!extractZip(zipPath, tempDir)) {
        QDir(tempDir).removeRecursively();
        cleanup();
        emit tunAssetsDownloadFailed(tr("Failed to extract hev-socks5-tunnel zip"));
        return;
    }

    // The zip contains a folder named "hev-socks5-tunnel" with the files
    QString extractedDir = tempDir + QLatin1String("/hev-socks5-tunnel");
    QStringList filesToCopy = {
        QStringLiteral("hev-socks5-tunnel.exe"),
        QStringLiteral("wintun.dll"),
        QStringLiteral("msys-2.0.dll")
    };

    bool allCopied = true;
    for (const QString &fileName : filesToCopy) {
        QString srcPath = extractedDir + QDir::separator() + fileName;
        QString dstPath = installDir + QDir::separator() + fileName;
        if (QFile::exists(srcPath)) {
            if (QFile::exists(dstPath)) QFile::remove(dstPath);
            if (!QFile::copy(srcPath, dstPath)) {
                if (m_logBuffer) m_logBuffer->append(tr("[TunAssets] WARNING: Failed to copy %1").arg(fileName));
                allCopied = false;
            } else {
                if (m_logBuffer) m_logBuffer->append(tr("[TunAssets] Installed %1").arg(dstPath));
            }
        } else {
            if (m_logBuffer) m_logBuffer->append(tr("[TunAssets] WARNING: %1 not found in archive").arg(fileName));
        }
    }

    QDir(tempDir).removeRecursively();
    delete m_downloadFile;
    m_downloadFile = nullptr;

    if (m_tunManager) m_tunManager->setTunBinaryPath(QString());
    m_downloadInProgress = false;

    if (!allCopied) {
        emit tunAssetsDownloadFailed(tr("Some files failed to install"));
    } else {
        emit tunAssetsDownloadFinished();
    }
#else
    // Non-Windows: Direct binary copy
    QString targetName = QStringLiteral("hev-socks5-tunnel");
    QString targetPath = installDir + QDir::separator() + targetName;
    if (QFile::exists(targetPath)) QFile::remove(targetPath);
    if (!QFile::copy(zipPath, targetPath)) {
        cleanup();
        emit tunAssetsDownloadFailed(tr("Failed to copy hev-socks5-tunnel to %1").arg(targetPath));
        return;
    }
    QFile::setPermissions(targetPath, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner | QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther | QFile::ExeOther);
    if (m_logBuffer) m_logBuffer->append(tr("[TunAssets] hev-socks5-tunnel installed at %1").arg(targetPath));
    if (m_tunManager) m_tunManager->setTunBinaryPath(QString());
    delete m_downloadFile;
    m_downloadFile = nullptr;
    m_downloadInProgress = false;
    emit tunAssetsDownloadFinished();
#endif
}

void TunAssetsManager::onWintunReleaseCheckFinished()
{
    if (!m_currentReply || m_step != Step::WintunCheck) return;
    if (m_currentReply->error() != QNetworkReply::NoError) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TunAssets] Wintun check failed, TUN may still work if wintun is installed."));
        m_downloadInProgress = false;
        emit tunAssetsDownloadFinished();
        return;
    }
    QByteArray data = m_currentReply->readAll();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    QString url = findWintunAssetUrl(data);
    if (url.isEmpty()) {
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TunAssets] No wintun zip found in releases."));
        m_downloadInProgress = false;
        emit tunAssetsDownloadFinished();
        return;
    }
    startWintunDownload(url);
}

void TunAssetsManager::startWintunDownload(const QString &url)
{
    m_step = Step::WintunDownload;
    m_downloadFile = new QTemporaryFile(this);
    if (!m_downloadFile->open()) {
        cleanup();
        m_downloadInProgress = false;
        emit tunAssetsDownloadFinished();
        return;
    }
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TunAssets] Downloading wintun..."));
    QNetworkRequest req{QUrl(url)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("PaqetN/1.0"));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(120000);
    m_currentReply = m_nam->get(req);
    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &TunAssetsManager::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished, this, &TunAssetsManager::onWintunDownloadFinished);
}

void TunAssetsManager::onWintunDownloadFinished()
{
    if (!m_currentReply || !m_downloadFile || m_step != Step::WintunDownload) return;
    if (m_currentReply->error() != QNetworkReply::NoError) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
        cleanup();
        m_downloadInProgress = false;
        emit tunAssetsDownloadFinished();
        return;
    }
    m_downloadFile->write(m_currentReply->readAll());
    m_downloadFile->flush();
    QString zipPath = m_downloadFile->fileName();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;
    delete m_downloadFile;
    m_downloadFile = nullptr;

    QString installDir = getInstallDir();
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QLatin1String("/paqetN_wintun");
    QDir().mkpath(tempDir);
    if (!extractZip(zipPath, tempDir)) {
        QDir(tempDir).removeRecursively();
        m_downloadInProgress = false;
        emit tunAssetsDownloadFinished();
        return;
    }
    // Find wintun.dll in extracted tree (e.g. wintun/bin/amd64/wintun.dll or amd64/wintun.dll)
    QString dllPath;
    QDirIterator it(tempDir, QStringList() << QStringLiteral("wintun.dll"), QDir::Files, QDirIterator::Subdirectories);
    if (it.hasNext()) dllPath = it.next();
    QDir(tempDir).removeRecursively();
    if (dllPath.isEmpty()) {
        m_downloadInProgress = false;
        emit tunAssetsDownloadFinished();
        return;
    }
    QString targetDll = installDir + QDir::separator() + QStringLiteral("wintun.dll");
    if (QFile::exists(targetDll)) QFile::remove(targetDll);
    QFile::copy(dllPath, targetDll);
    if (m_logBuffer) m_logBuffer->append(tr("[TunAssets] wintun.dll installed at %1").arg(targetDll));
    m_downloadInProgress = false;
    emit tunAssetsDownloadFinished();
}

void TunAssetsManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0)
        emit tunAssetsDownloadProgress(static_cast<int>((bytesReceived * 100) / bytesTotal));
}

void TunAssetsManager::cleanup()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    if (m_downloadFile) {
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
    }
    m_step = Step::None;
}
