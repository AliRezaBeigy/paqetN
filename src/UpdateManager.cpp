#include "UpdateManager.h"
#include "ZipExtractor.h"
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTemporaryFile>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QDirIterator>
#include <QProcess>
#include <QCoreApplication>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QDebug>
#include <QtConcurrent>
#include <QFutureWatcher>

namespace {
// Runs in a worker thread; never call on main thread to avoid UI lag.
QString fetchInstalledPaqetVersionInThread(const QString &appDirPath)
{
    const QString exeName =
#ifdef Q_OS_WIN
        QStringLiteral("paqet.exe");
#else
        QStringLiteral("paqet");
#endif
    const QString exePath = appDirPath + QDir::separator() + QStringLiteral("cores") + QDir::separator() + exeName;

    QProcess process;
    process.start(exePath, QStringList() << QStringLiteral("version"));

    if (!process.waitForFinished(3000)) {
        process.start(exeName, QStringList() << QStringLiteral("version"));
        if (!process.waitForFinished(3000))
            return QStringLiteral("Unknown");
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput()).trimmed();
    QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        if (line.startsWith(QStringLiteral("Version:"), Qt::CaseInsensitive)) {
            QString version = line.mid(8).trimmed();
            if (version.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
                version = version.mid(1);
            return version;
        }
    }
    return output.isEmpty() ? QStringLiteral("Unknown") : QStringLiteral("Unknown");
}
} // namespace

UpdateManager::UpdateManager(QObject *parent)
    : QObject(parent)
{
    m_nam = new QNetworkAccessManager(this);
}

UpdateManager::~UpdateManager()
{
    cancel();
}

QString UpdateManager::getPaqetNVersion() const
{
#ifdef PAQETN_VERSION
    return QStringLiteral(PAQETN_VERSION);
#else
    return QStringLiteral("0.1.0");
#endif
}

bool UpdateManager::isPaqetBinaryAvailable(const QString &customPath) const
{
    QString exeName =
#ifdef Q_OS_WIN
        QStringLiteral("paqet.exe");
#else
        QStringLiteral("paqet");
#endif

    if (!customPath.isEmpty()) {
        QFileInfo info(customPath);
        return info.exists() && info.isFile() && info.isExecutable();
    }

    // Check in cores subdirectory
    QString coresDirPath = QCoreApplication::applicationDirPath() + QDir::separator() + QStringLiteral("cores") + QDir::separator() + exeName;
    QFileInfo coresDirInfo(coresDirPath);
    if (coresDirInfo.exists() && coresDirInfo.isFile()) {
        return true;
    }

    // Check in system PATH
    QProcess process;
    process.start(exeName, QStringList{QStringLiteral("version")});
    if (process.waitForStarted(1000)) {
        process.kill();
        process.waitForFinished();
        return true;
    }

    return false;
}

QString UpdateManager::getInstalledPaqetVersion() const
{
    // Never block: return last known value (set by background fetch or after install).
    return m_installedPaqetVersion.isEmpty() ? QStringLiteral("Unknown") : m_installedPaqetVersion;
}

void UpdateManager::checkPaqetUpdate()
{
    if (m_currentReply) {
        emit paqetUpdateCheckFailed(tr("Another operation is in progress"));
        return;
    }

    m_checkingPaqetN = false;
    emit paqetUpdateCheckStarted();
    emit statusMessage(tr("Checking for paqet updates..."));

    QNetworkRequest req(QUrl(QStringLiteral("https://api.github.com/repos/hanselime/paqet/releases/latest")));
    req.setHeader(QNetworkRequest::UserAgentHeader, QString("PaqetN/%1").arg(getPaqetNVersion()));
    req.setTransferTimeout(6000); // 30 second timeout

    m_currentReply = m_nam->get(req);
    connect(m_currentReply, &QNetworkReply::finished, this, &UpdateManager::onReleaseCheckFinished);
}

void UpdateManager::checkPaqetNUpdate()
{
    if (m_currentReply) {
        emit paqetnUpdateCheckFailed(tr("Another operation is in progress"));
        return;
    }

    m_checkingPaqetN = true;
    emit paqetnUpdateCheckStarted();
    emit statusMessage(tr("Checking for PaqetN updates..."));

    QNetworkRequest req(QUrl(QStringLiteral("https://api.github.com/repos/AliRezaBeigy/paqetN/releases/latest")));
    req.setHeader(QNetworkRequest::UserAgentHeader, QString("PaqetN/%1").arg(getPaqetNVersion()));
    req.setTransferTimeout(6000);

    m_currentReply = m_nam->get(req);
    connect(m_currentReply, &QNetworkReply::finished, this, &UpdateManager::onReleaseCheckFinished);
}

void UpdateManager::downloadPaqet(const QString &version, const QString &downloadUrl)
{
    if (m_currentReply) {
        emit paqetDownloadFailed(tr("Another operation is in progress"));
        return;
    }

    m_downloadType = DownloadType::PaqetBinary;
    m_downloadingVersion = version;
    m_paqetDownloadUrl = downloadUrl;

    emit paqetDownloadStarted();
    emit statusMessage(tr("Downloading paqet %1...").arg(version));

    m_downloadFile = new QTemporaryFile(this);
    if (!m_downloadFile->open()) {
        emit paqetDownloadFailed(tr("Failed to create temporary file"));
        cleanup();
        return;
    }

    QNetworkRequest req{QUrl(downloadUrl)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QString("PaqetN/%1").arg(getPaqetNVersion()));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(300000);

    m_currentReply = m_nam->get(req);
    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &UpdateManager::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished, this, &UpdateManager::onDownloadFinished);
}

void UpdateManager::downloadPaqetNUpdate(const QString &version, const QString &downloadUrl)
{
    if (m_currentReply) {
        emit paqetnDownloadFailed(tr("Another operation is in progress"));
        return;
    }

    m_downloadType = DownloadType::PaqetNUpdate;
    m_downloadingVersion = version;

    emit paqetnDownloadStarted();
    emit statusMessage(tr("Downloading PaqetN %1...").arg(version));

    m_downloadFile = new QTemporaryFile(this);
    if (!m_downloadFile->open()) {
        emit paqetnDownloadFailed(tr("Failed to create temporary file"));
        cleanup();
        return;
    }

    QNetworkRequest req{QUrl(downloadUrl)};
    req.setHeader(QNetworkRequest::UserAgentHeader, QString("PaqetN/%1").arg(getPaqetNVersion()));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    req.setTransferTimeout(300000);

    m_currentReply = m_nam->get(req);
    connect(m_currentReply, &QNetworkReply::downloadProgress, this, &UpdateManager::onDownloadProgress);
    connect(m_currentReply, &QNetworkReply::finished, this, &UpdateManager::onDownloadFinished);
}

void UpdateManager::cancel()
{
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    cleanup();
}

void UpdateManager::onReleaseCheckFinished()
{
    if (!m_currentReply) return;

    bool isPaqetN = m_checkingPaqetN;

    if (m_currentReply->error() != QNetworkReply::NoError) {
        QString errorMsg = m_currentReply->errorString();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;

        if (isPaqetN) {
            emit paqetnUpdateCheckFailed(errorMsg);
        } else {
            emit paqetUpdateCheckFailed(errorMsg);
        }
        emit statusMessage(tr("Update check failed: %1").arg(errorMsg));
        return;
    }

    QByteArray data = m_currentReply->readAll();
    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        QString error = tr("Invalid JSON response from GitHub");
        if (isPaqetN) {
            emit paqetnUpdateCheckFailed(error);
        } else {
            emit paqetUpdateCheckFailed(error);
        }
        emit statusMessage(error);
        return;
    }

    QJsonObject obj = doc.object();
    QString tagName = obj[QStringLiteral("tag_name")].toString();

    if (tagName.isEmpty()) {
        QString error = tr("No release found");
        if (isPaqetN) {
            emit paqetnUpdateCheckFailed(error);
        } else {
            emit paqetUpdateCheckFailed(error);
        }
        emit statusMessage(error);
        return;
    }

    if (isPaqetN) {
        // For PaqetN, we need Windows exe
        QJsonArray assets = obj[QStringLiteral("assets")].toArray();
        QString downloadUrl;

        for (const QJsonValue &val : assets) {
            QJsonObject asset = val.toObject();
            QString name = asset[QStringLiteral("name")].toString();

#ifdef Q_OS_WIN
            if (name.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive) &&
                name.contains(QStringLiteral("paqetN"), Qt::CaseInsensitive)) {
                downloadUrl = asset[QStringLiteral("browser_download_url")].toString();
                break;
            }
#endif
        }

        if (downloadUrl.isEmpty()) {
            emit paqetnUpdateCheckFailed(tr("No compatible PaqetN executable found for your platform"));
            emit statusMessage(tr("No PaqetN update available for your platform"));
            return;
        }

        // Compare installed version with latest version
        QString installedVersion = getPaqetNVersion();
        QString latestVersion = tagName;
        
        // Strip 'v' prefix from tagName if present for comparison
        if (latestVersion.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
            latestVersion = latestVersion.mid(1);
        }
        
        qDebug() << "[UpdateManager] Comparing PaqetN versions - Installed:" << installedVersion << "Latest:" << latestVersion;
        
        // Only report update if latest version is newer than installed version
        int comparison = compareVersions(installedVersion, latestVersion);
        if (comparison < 0) {
            // Latest version is newer
            qDebug() << "[UpdateManager] PaqetN update available: installed" << installedVersion << "< latest" << latestVersion;
            emit paqetnUpdateCheckFinished(true, tagName, downloadUrl);
            emit statusMessage(tr("PaqetN update available: %1").arg(tagName));
        } else {
            // Same or older version
            qDebug() << "[UpdateManager] PaqetN is up to date: installed" << installedVersion << ">= latest" << latestVersion;
            emit paqetnUpdateCheckFinished(false, tagName, downloadUrl);
            emit statusMessage(tr("PaqetN is up to date (version %1)").arg(installedVersion));
        }
    } else {
        // For paqet binary
        QString platform = detectPlatform();
        if (platform.isEmpty()) {
            emit paqetUpdateCheckFailed(tr("Platform detection failed"));
            emit statusMessage(tr("Platform detection failed. Please download manually from GitHub."));
            return;
        }

        QString downloadUrl = findAssetUrl(data, platform);
        if (downloadUrl.isEmpty()) {
            emit paqetUpdateCheckFailed(tr("No compatible binary found for platform: %1").arg(platform));
            emit statusMessage(tr("No paqet update available for your platform"));
            return;
        }

        // Fetch installed version in background to avoid blocking the UI
        QString appDir = QCoreApplication::applicationDirPath();
        QFutureWatcher<QString> *watcher = new QFutureWatcher<QString>(this);
        connect(watcher, &QFutureWatcher<QString>::finished, this, [this, watcher, tagName, downloadUrl]() {
            watcher->deleteLater();
            QString installedVersion = watcher->result();
            m_installedPaqetVersion = installedVersion;
            emit installedPaqetVersionChanged(installedVersion);

            QString latestVersion = tagName;
            if (latestVersion.startsWith(QLatin1Char('v'), Qt::CaseInsensitive))
                latestVersion = latestVersion.mid(1);

            qDebug() << "[UpdateManager] Comparing versions - Installed:" << installedVersion << "Latest:" << latestVersion;

            if (installedVersion == QStringLiteral("Unknown")) {
                qDebug() << "[UpdateManager] Installed version unknown, assuming update available";
                emit paqetUpdateCheckFinished(true, tagName, downloadUrl);
                emit statusMessage(tr("Paqet update available: %1").arg(tagName));
            } else {
                int comparison = compareVersions(installedVersion, latestVersion);
                if (comparison < 0) {
                    qDebug() << "[UpdateManager] Update available: installed" << installedVersion << "< latest" << latestVersion;
                    emit paqetUpdateCheckFinished(true, tagName, downloadUrl);
                    emit statusMessage(tr("Paqet update available: %1").arg(tagName));
                } else {
                    qDebug() << "[UpdateManager] No update needed: installed" << installedVersion << ">= latest" << latestVersion;
                    emit paqetUpdateCheckFinished(false, tagName, downloadUrl);
                    emit statusMessage(tr("Paqet is up to date (version %1)").arg(installedVersion));
                }
            }
        });
        watcher->setFuture(QtConcurrent::run(fetchInstalledPaqetVersionInThread, appDir));
    }
}

void UpdateManager::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (m_downloadType == DownloadType::PaqetBinary) {
        emit paqetDownloadProgress(bytesReceived, bytesTotal);
    } else if (m_downloadType == DownloadType::PaqetNUpdate) {
        emit paqetnDownloadProgress(bytesReceived, bytesTotal);
    }
}

void UpdateManager::onDownloadFinished()
{
    if (!m_currentReply || !m_downloadFile) return;

    qDebug() << "[UpdateManager] Download finished";

    if (m_currentReply->error() != QNetworkReply::NoError) {
        QString errorMsg = m_currentReply->errorString();
        qWarning() << "[UpdateManager] Download error:" << errorMsg;

        if (m_downloadType == DownloadType::PaqetBinary) {
            emit paqetDownloadFailed(errorMsg);
        } else if (m_downloadType == DownloadType::PaqetNUpdate) {
            emit paqetnDownloadFailed(errorMsg);
        }

        emit statusMessage(tr("Download failed: %1").arg(errorMsg));
        cleanup();
        return;
    }

    // Write remaining data
    m_downloadFile->write(m_currentReply->readAll());
    m_downloadFile->flush();

    // Verify file size
    qint64 expectedSize = m_currentReply->header(QNetworkRequest::ContentLengthHeader).toLongLong();
    qint64 actualSize = m_downloadFile->size();

    qDebug() << "[UpdateManager] Downloaded file size:" << actualSize << "bytes (expected:" << expectedSize << ")";

    if (expectedSize > 0 && actualSize != expectedSize) {
        QString error = tr("Download incomplete: expected %1 bytes, got %2 bytes").arg(expectedSize).arg(actualSize);
        qWarning() << "[UpdateManager]" << error;

        if (m_downloadType == DownloadType::PaqetBinary) {
            emit paqetDownloadFailed(error);
        } else if (m_downloadType == DownloadType::PaqetNUpdate) {
            emit paqetnDownloadFailed(error);
        }

        emit statusMessage(error);
        cleanup();
        return;
    }

    QString downloadedFilePath = m_downloadFile->fileName();
    qDebug() << "[UpdateManager] Download saved to:" << downloadedFilePath;

    m_currentReply->deleteLater();
    m_currentReply = nullptr;

    if (m_downloadType == DownloadType::PaqetBinary) {
        // Extract paqet binary
        QString installDir = getPaqetInstallDir();
        qDebug() << "[UpdateManager] Installing to:" << installDir;
        QDir().mkpath(installDir);

        const bool isTarGz = m_paqetDownloadUrl.endsWith(QLatin1String(".tar.gz"), Qt::CaseInsensitive);
        bool extractOk = isTarGz ? extractTarGz(downloadedFilePath, installDir) : extractZip(downloadedFilePath, installDir);
        if (!extractOk) {
            QString error = tr("Failed to extract downloaded file");
            qWarning() << "[UpdateManager]" << error;
            emit paqetDownloadFailed(error);
            cleanup();
            return;
        }

        QString targetExeName =
#ifdef Q_OS_WIN
            QStringLiteral("paqet.exe");
#else
            QStringLiteral("paqet");
#endif

        // Search patterns for actual GitHub release binary names
        QStringList searchPatterns;
#ifdef Q_OS_WIN
        searchPatterns << QStringLiteral("paqet_windows_amd64.exe") << QStringLiteral("paqet.exe");
#elif defined(Q_OS_MACOS)
        searchPatterns << QStringLiteral("paqet_darwin_amd64") << QStringLiteral("paqet_darwin_arm64") << QStringLiteral("paqet");
#else // Linux
        searchPatterns << QStringLiteral("paqet_linux_amd64") << QStringLiteral("paqet_linux_arm64") << QStringLiteral("paqet_linux_arm32") << QStringLiteral("paqet");
#endif

        // Find paqet binary (might be in subdirectory or with platform-specific name)
        QString installedPath;
        QString foundBinaryPath;

        for (const QString &pattern : searchPatterns) {
            QDirIterator it(installDir, QStringList() << pattern, QDir::Files, QDirIterator::Subdirectories);
            if (it.hasNext()) {
                foundBinaryPath = it.next();
                qDebug() << "[UpdateManager] Found paqet binary at:" << foundBinaryPath << "matching pattern:" << pattern;
                break;
            }
        }

        if (!foundBinaryPath.isEmpty()) {
            // Rename to standard "paqet" or "paqet.exe" in install dir root
            QString targetPath = installDir + QDir::separator() + targetExeName;
            QFile::remove(targetPath); // Remove if exists

            if (QFile::rename(foundBinaryPath, targetPath)) {
                installedPath = targetPath;
                qDebug() << "[UpdateManager] Renamed paqet binary to:" << targetPath;
            } else {
                installedPath = foundBinaryPath;
                qWarning() << "[UpdateManager] Failed to rename, using original path:" << foundBinaryPath;
            }
        } else {
            installedPath = installDir + QDir::separator() + targetExeName;
            qWarning() << "[UpdateManager] Warning: paqet binary not found after extraction, expected at:" << installedPath;
        }

#ifndef Q_OS_WIN
        // Set executable permission on Unix-like systems
        QFile::setPermissions(installedPath,
                              QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                              QFile::ReadGroup | QFile::ExeGroup |
                              QFile::ReadOther | QFile::ExeOther);
#endif

        m_installedPaqetVersion = m_downloadingVersion.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)
            ? m_downloadingVersion.mid(1) : m_downloadingVersion;
        emit paqetDownloadFinished(installedPath);
        emit statusMessage(tr("Paqet %1 installed successfully").arg(m_downloadingVersion));
        emit installedPaqetVersionChanged(m_installedPaqetVersion);
        cleanup();

    } else if (m_downloadType == DownloadType::PaqetNUpdate) {
        // Perform self-update
        performSelfUpdate(downloadedFilePath);
    }
}

QString UpdateManager::detectPlatform() const
{
#ifdef Q_OS_WIN
    return QStringLiteral("windows-amd64");
#elif defined(Q_OS_MACOS)
    // TODO: detect ARM vs Intel
    return QStringLiteral("darwin-amd64");
#elif defined(Q_OS_LINUX)
    // TODO: detect ARM vs x86
    return QStringLiteral("linux-amd64");
#else
    return QString();
#endif
}

QString UpdateManager::findAssetUrl(const QByteArray &jsonData, const QString &platform) const
{
    QJsonDocument doc = QJsonDocument::fromJson(jsonData);
    if (!doc.isObject()) {
        return QString();
    }

    QJsonObject obj = doc.object();
    QJsonArray assets = obj[QStringLiteral("assets")].toArray();

    QString pattern = QString("paqet-%1-").arg(platform);

    for (const QJsonValue &val : assets) {
        QJsonObject asset = val.toObject();
        QString name = asset[QStringLiteral("name")].toString();

        if (name.contains(pattern, Qt::CaseInsensitive)) {
            return asset[QStringLiteral("browser_download_url")].toString();
        }
    }

    return QString();
}

bool UpdateManager::extractZip(const QString &zipPath, const QString &destDir)
{
    qDebug() << "[UpdateManager] Extracting ZIP:" << zipPath << "to" << destDir;

    // Verify ZIP file exists
    QFileInfo zipInfo(zipPath);
    if (!zipInfo.exists()) {
        QString error = tr("ZIP file does not exist: %1").arg(zipPath);
        qWarning() << "[UpdateManager]" << error;
        emit statusMessage(error);
        return false;
    }

    qDebug() << "[UpdateManager] ZIP file size:" << zipInfo.size() << "bytes";

    // Use cross-platform ZIP extractor
    QString errorMsg;
    emit statusMessage(tr("Extracting files..."));

    bool success = ZipExtractor::extractFile(zipPath, destDir, errorMsg);

    if (!success) {
        QString error = tr("Extraction failed: %1").arg(errorMsg);
        qWarning() << "[UpdateManager]" << error;
        emit statusMessage(error);
        return false;
    }

    qDebug() << "[UpdateManager] Extraction completed successfully";
    emit statusMessage(tr("Extraction completed"));
    return true;
}

bool UpdateManager::extractTarGz(const QString &archivePath, const QString &destDir)
{
    qDebug() << "[UpdateManager] Extracting tar.gz:" << archivePath << "to" << destDir;

    QFileInfo archiveInfo(archivePath);
    if (!archiveInfo.exists()) {
        QString error = tr("Archive does not exist: %1").arg(archivePath);
        qWarning() << "[UpdateManager]" << error;
        emit statusMessage(error);
        return false;
    }

    qDebug() << "[UpdateManager] Archive file size:" << archiveInfo.size() << "bytes";
    emit statusMessage(tr("Extracting files..."));

    QDir dir(destDir);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    QProcess tar;
    tar.setProgram(QStringLiteral("tar"));
    tar.setArguments(QStringList() << QStringLiteral("-xzf") << archivePath << QStringLiteral("-C") << destDir);
    tar.start();
    if (!tar.waitForFinished(60000)) {
        QString error = tr("Failed to extract tar.gz: %1").arg(tar.errorString());
        qWarning() << "[UpdateManager]" << error;
        emit statusMessage(error);
        return false;
    }
    if (tar.exitStatus() != QProcess::NormalExit || tar.exitCode() != 0) {
        QString errOut = QString::fromUtf8(tar.readAllStandardError()).trimmed();
        QString error = tr("Failed to extract tar.gz (exit %1): %2").arg(tar.exitCode()).arg(errOut.isEmpty() ? tar.errorString() : errOut);
        qWarning() << "[UpdateManager]" << error;
        emit statusMessage(error);
        return false;
    }

    qDebug() << "[UpdateManager] tar.gz extraction completed successfully";
    emit statusMessage(tr("Extraction completed"));
    return true;
}

QString UpdateManager::getPaqetInstallDir() const
{
    // Install in cores subdirectory of application directory
    return QCoreApplication::applicationDirPath() + QDir::separator() + QStringLiteral("cores");
}

void UpdateManager::performSelfUpdate(const QString &newExePath)
{
    QString currentExe = QCoreApplication::applicationFilePath();
    QString appDir = QCoreApplication::applicationDirPath();
    QString tempDir = QStandardPaths::writableLocation(QStandardPaths::TempLocation);
    QString updateExe = tempDir + QDir::separator() + QStringLiteral("PaqetN_update.exe");
    QString updateScript = tempDir + QDir::separator() + QStringLiteral("paqetn_update.bat");

    // Copy downloaded exe to temp with known name
    if (!QFile::copy(newExePath, updateExe)) {
        // Try to remove existing and copy again
        QFile::remove(updateExe);
        if (!QFile::copy(newExePath, updateExe)) {
            emit paqetnDownloadFailed(tr("Cannot copy update file to temp location"));
            cleanup();
            return;
        }
    }

    // Create backup
    QString backupExe = currentExe + QStringLiteral(".backup");
    QFile::remove(backupExe); // Remove old backup if exists

    // Create update batch script
    QFile scriptFile(updateScript);
    if (!scriptFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit paqetnDownloadFailed(tr("Cannot create update script"));
        cleanup();
        return;
    }

    QTextStream out(&scriptFile);
    out << "@echo off\n";
    out << "echo Waiting for PaqetN to close...\n";
    out << "timeout /t 2 /nobreak > nul\n";
    out << "echo Creating backup...\n";
    out << "copy /Y \"" << currentExe << "\" \"" << backupExe << "\"\n";
    out << "echo Installing update...\n";
    out << "move /Y \"" << updateExe << "\" \"" << currentExe << "\"\n";
    out << "if errorlevel 1 (\n";
    out << "    echo Update failed, restoring backup...\n";
    out << "    copy /Y \"" << backupExe << "\" \"" << currentExe << "\"\n";
    out << "    pause\n";
    out << "    exit /b 1\n";
    out << ")\n";
    out << "echo Starting PaqetN...\n";
    out << "start \"\" \"" << currentExe << "\"\n";
    out << "del \"%~f0\"\n";
    scriptFile.close();

    emit statusMessage(tr("Restarting to apply update..."));
    emit paqetnDownloadFinished(); // Signal to close app

    // Start update script in detached mode
    QProcess::startDetached(QStringLiteral("cmd.exe"),
                            QStringList() << QStringLiteral("/c") << updateScript,
                            appDir);

    cleanup();
}

void UpdateManager::cleanup()
{
    if (m_currentReply) {
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }

    if (m_downloadFile) {
        m_downloadFile->deleteLater();
        m_downloadFile = nullptr;
    }

    m_downloadType = DownloadType::None;
    m_downloadingVersion.clear();
    m_paqetDownloadUrl.clear();
}

int UpdateManager::compareVersions(const QString &v1, const QString &v2) const
{
    // Normalize versions by stripping 'v' prefix
    QString version1 = v1;
    QString version2 = v2;
    
    if (version1.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        version1 = version1.mid(1);
    }
    if (version2.startsWith(QLatin1Char('v'), Qt::CaseInsensitive)) {
        version2 = version2.mid(1);
    }
    
    // Split into main version and pre-release parts
    // e.g., "1.0.0-alpha.15" -> main: "1.0.0", prerelease: "alpha.15"
    QStringList parts1 = version1.split(QLatin1Char('-'), Qt::SkipEmptyParts);
    QStringList parts2 = version2.split(QLatin1Char('-'), Qt::SkipEmptyParts);
    
    QString main1 = parts1.isEmpty() ? version1 : parts1[0];
    QString main2 = parts2.isEmpty() ? version2 : parts2[0];
    
    QString prerelease1 = parts1.size() > 1 ? parts1[1] : QString();
    QString prerelease2 = parts2.size() > 1 ? parts2[1] : QString();
    
    // Compare main version numbers (semantic versioning: MAJOR.MINOR.PATCH)
    QStringList nums1 = main1.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    QStringList nums2 = main2.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    
    // Ensure both have at least 3 components (pad with zeros if needed)
    while (nums1.size() < 3) nums1.append(QStringLiteral("0"));
    while (nums2.size() < 3) nums2.append(QStringLiteral("0"));
    
    // Compare major, minor, patch
    for (int i = 0; i < 3; ++i) {
        bool ok1, ok2;
        int num1 = nums1[i].toInt(&ok1);
        int num2 = nums2[i].toInt(&ok2);
        
        if (!ok1 || !ok2) {
            // If parsing fails, do string comparison
            if (nums1[i] < nums2[i]) return -1;
            if (nums1[i] > nums2[i]) return 1;
            continue;
        }
        
        if (num1 < num2) return -1;
        if (num1 > num2) return 1;
    }
    
    // Main versions are equal, compare pre-release identifiers
    // A version without a pre-release is considered newer than one with a pre-release
    if (prerelease1.isEmpty() && prerelease2.isEmpty()) {
        return 0; // Both are release versions, equal
    }
    if (prerelease1.isEmpty()) {
        return 1; // v1 is release, v2 is pre-release, v1 is newer
    }
    if (prerelease2.isEmpty()) {
        return -1; // v1 is pre-release, v2 is release, v2 is newer
    }
    
    // Both have pre-release identifiers, compare them lexicographically
    // Split by dots for comparison (e.g., "alpha.15" -> ["alpha", "15"])
    QStringList pre1 = prerelease1.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    QStringList pre2 = prerelease2.split(QLatin1Char('.'), Qt::SkipEmptyParts);
    
    int minLen = qMin(pre1.size(), pre2.size());
    for (int i = 0; i < minLen; ++i) {
        // Try numeric comparison first
        bool ok1, ok2;
        int num1 = pre1[i].toInt(&ok1);
        int num2 = pre2[i].toInt(&ok2);
        
        if (ok1 && ok2) {
            // Both are numeric
            if (num1 < num2) return -1;
            if (num1 > num2) return 1;
        } else {
            // At least one is non-numeric, compare as strings
            if (pre1[i] < pre2[i]) return -1;
            if (pre1[i] > pre2[i]) return 1;
        }
    }
    
    // If one has more components, it's considered newer if they're numeric
    if (pre1.size() < pre2.size()) return -1;
    if (pre1.size() > pre2.size()) return 1;
    
    return 0; // Versions are equal
}
