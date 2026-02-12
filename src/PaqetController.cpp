#include "PaqetController.h"
#include "PaqetConfig.h"
#include "LogBuffer.h"
#include "PaqetRunner.h"
#include "LatencyChecker.h"
#include "UpdateManager.h"
#include "NetworkInfoDetector.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QFile>
#include <QTimer>

PaqetController::PaqetController(QObject *parent) : QObject(parent) {
    m_repo = new ConfigRepository(this);
    m_settings = new SettingsRepository(this);
    m_configList = new ConfigListModel(this);
    m_logBuffer = new LogBuffer(this);
    m_runner = new PaqetRunner(m_logBuffer, this);
    m_latencyChecker = new LatencyChecker(this);
    m_updateManager = new UpdateManager(this);

    connect(m_repo, &ConfigRepository::configsChanged, this, &PaqetController::reloadConfigList);
    connect(m_runner, &PaqetRunner::runningChanged, this, &PaqetController::isRunningChanged);
    connect(m_logBuffer, &LogBuffer::logAppended, this, &PaqetController::logTextChanged);
    connect(m_latencyChecker, &LatencyChecker::result, this, [this](int ms) {
        m_latencyMs = ms;
        m_latencyTesting = false;
        emit latencyMsChanged();
        emit latencyTestingChanged();
    });
    connect(m_latencyChecker, &LatencyChecker::started, this, [this] {
        m_latencyTesting = true;
        emit latencyTestingChanged();
    });

    // Connect UpdateManager signals
    connect(m_updateManager, &UpdateManager::paqetUpdateCheckStarted, this, [this] {
        m_updateCheckInProgress = true;
        emit updateCheckInProgressChanged();
    });
    connect(m_updateManager, &UpdateManager::paqetUpdateCheckFinished, this, &PaqetController::onPaqetUpdateCheckFinished);
    connect(m_updateManager, &UpdateManager::paqetUpdateCheckFailed, this, [this](const QString &error) {
        m_updateCheckInProgress = false;
        m_updateStatusMessage = error;
        emit updateCheckInProgressChanged();
        emit updateStatusMessageChanged();
    });
    connect(m_updateManager, &UpdateManager::paqetnUpdateCheckStarted, this, [this] {
        m_updateCheckInProgress = true;
        emit updateCheckInProgressChanged();
    });
    connect(m_updateManager, &UpdateManager::paqetnUpdateCheckFinished, this, &PaqetController::onPaqetNUpdateCheckFinished);
    connect(m_updateManager, &UpdateManager::paqetnUpdateCheckFailed, this, [this](const QString &error) {
        m_updateCheckInProgress = false;
        m_updateStatusMessage = error;
        emit updateCheckInProgressChanged();
        emit updateStatusMessageChanged();
    });
    connect(m_updateManager, &UpdateManager::paqetDownloadStarted, this, [this] {
        m_paqetDownloadInProgress = true;
        m_paqetDownloadProgress = 0;
        emit paqetDownloadInProgressChanged();
        emit paqetDownloadProgressChanged();
    });
    connect(m_updateManager, &UpdateManager::paqetDownloadProgress, this, &PaqetController::onPaqetDownloadProgress);
    connect(m_updateManager, &UpdateManager::paqetDownloadFinished, this, &PaqetController::onPaqetDownloadFinished);
    connect(m_updateManager, &UpdateManager::paqetDownloadFailed, this, [this](const QString &error) {
        m_paqetDownloadInProgress = false;
        m_updateStatusMessage = error;
        emit paqetDownloadInProgressChanged();
        emit updateStatusMessageChanged();
    });
    connect(m_updateManager, &UpdateManager::paqetnDownloadStarted, this, [this] {
        m_paqetnDownloadInProgress = true;
        m_paqetnDownloadProgress = 0;
        emit paqetnDownloadInProgressChanged();
        emit paqetnDownloadProgressChanged();
    });
    connect(m_updateManager, &UpdateManager::paqetnDownloadProgress, this, &PaqetController::onPaqetNDownloadProgress);
    connect(m_updateManager, &UpdateManager::paqetnDownloadFinished, this, &PaqetController::onPaqetNDownloadFinished);
    connect(m_updateManager, &UpdateManager::paqetnDownloadFailed, this, [this](const QString &error) {
        m_paqetnDownloadInProgress = false;
        m_updateStatusMessage = error;
        emit paqetnDownloadInProgressChanged();
        emit updateStatusMessageChanged();
    });
    connect(m_updateManager, &UpdateManager::statusMessage, this, [this](const QString &msg) {
        m_updateStatusMessage = msg;
        emit updateStatusMessageChanged();
    });
    connect(m_updateManager, &UpdateManager::installedPaqetVersionChanged, this, &PaqetController::installedPaqetVersionChanged);

    m_selectedConfigId = m_repo->lastSelectedId();
    reloadConfigList();

    // Prompt user to download paqet if missing
    QTimer::singleShot(500, this, [this] {
        if (m_settings->autoDownloadPaqet()) {
            if (!m_updateManager->isPaqetBinaryAvailable(m_settings->paqetBinaryPath())) {
                emit paqetBinaryMissingPrompt();
            }
        }
    });
}

PaqetController::~PaqetController() = default;

void PaqetController::setSelectedConfigId(const QString &id) {
    if (m_selectedConfigId == id) return;
    m_selectedConfigId = id;
    m_repo->setLastSelectedId(id);
    emit selectedConfigIdChanged();
}

QString PaqetController::selectedConfigName() const {
    PaqetConfig c = selectedConfig();
    return c.name.isEmpty() ? c.serverAddr : c.name;
}

bool PaqetController::isRunning() const {
    return m_runner && m_runner->isRunning();
}

QString PaqetController::logText() const {
    return m_logBuffer ? m_logBuffer->fullText() : QString();
}

void PaqetController::reloadConfigList() {
    m_configList->setConfigs(m_repo->configs());
    emit configsChanged();
}

PaqetConfig PaqetController::selectedConfig() const {
    if (m_selectedConfigId.isEmpty()) return PaqetConfig();
    return m_repo->getById(m_selectedConfigId);
}

QVariantMap PaqetController::selectedConfigData() const {
    PaqetConfig c = selectedConfig();
    if (c.id.isEmpty()) return QVariantMap();
    return c.toVariantMap();
}

QVariantList PaqetController::getGroups() {
    return m_configList->distinctGroups();
}

QVariantMap PaqetController::getConfigForEdit(const QString &id) {
    if (id.isEmpty()) {
        PaqetConfig c;
        c.socksListen = QStringLiteral("127.0.0.1:%1").arg(m_settings->socksPort());
        // Network settings are auto-detected at connect time, not stored in config
        return c.toVariantMap();
    }
    return m_repo->getById(id).toVariantMap();
}

void PaqetController::saveConfig(const QVariantMap &config) {
    PaqetConfig c = PaqetConfig::fromVariantMap(config);
    if (c.id.isEmpty()) {
        QString newId = m_repo->add(c);
        if (!newId.isEmpty()) setSelectedConfigId(newId);
    } else {
        m_repo->update(c);
    }
}

void PaqetController::deleteConfig(const QString &id) {
    if (m_connectedConfigId == id) disconnect();
    m_repo->remove(id);
    if (m_selectedConfigId == id) setSelectedConfigId(QString());
}

void PaqetController::renameGroup(const QString &oldName, const QString &newName) {
    m_repo->renameGroup(oldName, newName);
}

bool PaqetController::addConfigFromImport(const QString &text) {
    auto opt = PaqetConfig::parseFromImport(text);
    if (!opt) return false;
    PaqetConfig c = *opt;
    if (c.socksListen.isEmpty() || c.socksListen == QLatin1String("127.0.0.1:1284"))
        c.socksListen = QStringLiteral("127.0.0.1:%1").arg(m_settings->socksPort());
    QString newId = m_repo->add(c);
    if (!newId.isEmpty()) setSelectedConfigId(newId);
    return true;
}

QString PaqetController::exportPaqetUri(const QString &id) {
    return m_repo->getById(id).toPaqetUri();
}

QString PaqetController::exportYaml(const QString &id) {
    return m_repo->getById(id).toYaml(m_settings->logLevel());
}

void PaqetController::connectToSelected() {
    PaqetConfig c = selectedConfig();
    if (c.id.isEmpty()) {
        m_logBuffer->append(tr("[PaqetN] ERROR: No config selected"));
        return;
    }

    m_logBuffer->append(tr("[PaqetN] Attempting to connect to: %1").arg(c.name.isEmpty() ? c.serverAddr : c.name));

    // Auto-detect network info at connect time (always detect fresh)
    m_logBuffer->append(tr("[PaqetN] Auto-detecting network adapter..."));
    NetworkInfoDetector detector;
    detector.setLogBuffer(m_logBuffer);
    detector.setLogLevel(m_settings->logLevel());  // Respect log level setting
    auto adapter = detector.getDefaultAdapter();
    
    if (!adapter.name.isEmpty() && !adapter.ipv4Address.isEmpty()) {
        c.guid = adapter.guid;
#ifdef Q_OS_WIN
        // On Windows, use the adapter friendly name (e.g., "Ethernet") for interface field
        c.networkInterface = adapter.name;
#else
        // On Unix, use the interface name (e.g., "eth0", "en0")
        c.networkInterface = adapter.interfaceName.isEmpty() ? adapter.name : adapter.interfaceName;
#endif
        c.ipv4Addr = adapter.ipv4Address;
        c.routerMac = adapter.gatewayMac.isEmpty() ? QStringLiteral("00:00:00:00:00:00") : adapter.gatewayMac;
        m_logBuffer->append(tr("[PaqetN] Network adapter detected: %1, IP: %2, Gateway: %3")
            .arg(adapter.name, adapter.ipv4Address, adapter.gatewayIp));
    } else {
        m_logBuffer->append(tr("[PaqetN] WARNING: Could not detect network adapter, using defaults"));
        // Set defaults based on platform
#ifdef Q_OS_WIN
        c.guid = QString();  // Will be detected or user must provide
        c.networkInterface = QString();
#else
        c.networkInterface = QStringLiteral("lo");
        c.guid = QString();
#endif
        c.ipv4Addr = QStringLiteral("127.0.0.1:0");
        c.routerMac = QStringLiteral("00:00:00:00:00:00");
    }

    // Check if paqet binary is available
    QString binaryPath = m_settings->paqetBinaryPath();
    m_logBuffer->append(tr("[PaqetN] Binary path: %1").arg(binaryPath));

    if (!m_updateManager || !m_updateManager->isPaqetBinaryAvailable(binaryPath)) {
        m_logBuffer->append(tr("[PaqetN] ERROR: Paqet binary not found at: %1").arg(binaryPath));
        m_logBuffer->append(tr("[PaqetN] Please download it from the Updates page."));
        emit paqetBinaryMissing();
        return;
    }

    m_logBuffer->append(tr("[PaqetN] Binary found, setting path..."));
    m_runner->setPaqetBinaryPath(binaryPath);

    m_logBuffer->append(tr("[PaqetN] Starting paqet with log level: %1").arg(m_settings->logLevel()));
    if (m_runner->start(c, m_settings->logLevel())) {
        m_connectedConfigId = c.id;
        m_logBuffer->append(tr("[PaqetN] Connection initiated successfully"));
    } else {
        m_logBuffer->append(tr("[PaqetN] ERROR: Failed to start paqet process"));
    }
}

void PaqetController::disconnect() {
    m_runner->stop();
    m_connectedConfigId.clear();
    m_latencyMs = -1;
    emit latencyMsChanged();
}

void PaqetController::testLatency() {
    PaqetConfig c = selectedConfig();
    if (c.id.isEmpty()) return;
    m_latencyChecker->check(c.socksPort(), m_settings->connectionCheckUrl());
}

void PaqetController::clearLog() {
    if (m_logBuffer) m_logBuffer->clear();
}

void PaqetController::copyToClipboard(const QString &text) {
    QClipboard *cb = QGuiApplication::clipboard();
    if (cb) cb->setText(text);
}

QString PaqetController::getClipboardText() const {
    QClipboard *cb = QGuiApplication::clipboard();
    return cb ? cb->text() : QString();
}

QString PaqetController::readFile(const QString &path) const {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return QString();
    return QString::fromUtf8(f.readAll());
}

bool PaqetController::writeFile(const QString &path, const QString &content) const {
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return false;
    return f.write(content.toUtf8()) >= 0;
}

QString PaqetController::getTheme() const { return m_settings->theme(); }
void PaqetController::setTheme(const QString &theme) { m_settings->setTheme(theme); }
int PaqetController::getSocksPort() const { return m_settings->socksPort(); }
void PaqetController::setSocksPort(int port) { m_settings->setSocksPort(port); }
QString PaqetController::getConnectionCheckUrl() const { return m_settings->connectionCheckUrl(); }
void PaqetController::setConnectionCheckUrl(const QString &url) { m_settings->setConnectionCheckUrl(url); }
int PaqetController::getConnectionCheckTimeoutSeconds() const { return m_settings->connectionCheckTimeoutSeconds(); }
void PaqetController::setConnectionCheckTimeoutSeconds(int seconds) { m_settings->setConnectionCheckTimeoutSeconds(seconds); }
bool PaqetController::getShowLatencyInUi() const { return m_settings->showLatencyInUi(); }
void PaqetController::setShowLatencyInUi(bool show) { m_settings->setShowLatencyInUi(show); }
QString PaqetController::getLogLevel() const { return m_settings->logLevel(); }
void PaqetController::setLogLevel(const QString &level) { m_settings->setLogLevel(level); }
QString PaqetController::getPaqetBinaryPath() const { return m_settings->paqetBinaryPath(); }
void PaqetController::setPaqetBinaryPath(const QString &path) { m_settings->setPaqetBinaryPath(path); }
QStringList PaqetController::getLogLevels() const { return SettingsRepository::logLevels(); }

// Update Manager methods
QString PaqetController::getPaqetNVersion() const {
    return m_updateManager ? m_updateManager->getPaqetNVersion() : QStringLiteral("0.1");
}

QString PaqetController::getInstalledPaqetVersion() {
    return m_updateManager ? m_updateManager->getInstalledPaqetVersion() : QStringLiteral("Unknown");
}

QString PaqetController::installedPaqetVersion() const {
    return m_updateManager ? m_updateManager->getInstalledPaqetVersion() : QStringLiteral("Unknown");
}

void PaqetController::checkPaqetUpdate() {
    m_autoDownloadMode = false; // User-initiated check
    if (m_updateManager) m_updateManager->checkPaqetUpdate();
}

void PaqetController::checkPaqetNUpdate() {
    if (m_updateManager) m_updateManager->checkPaqetNUpdate();
}

void PaqetController::downloadPaqet(const QString &version, const QString &url) {
    if (m_updateManager) m_updateManager->downloadPaqet(version, url);
}

void PaqetController::downloadPaqetNUpdate(const QString &version, const QString &url) {
    if (m_updateManager) m_updateManager->downloadPaqetNUpdate(version, url);
}

void PaqetController::cancelUpdate() {
    if (m_updateManager) m_updateManager->cancel();
}

bool PaqetController::isPaqetBinaryAvailable() const {
    return m_updateManager ? m_updateManager->isPaqetBinaryAvailable(m_settings->paqetBinaryPath()) : false;
}

void PaqetController::autoDownloadPaqetIfMissing() {
    if (!m_updateManager) return;
    m_autoDownloadMode = true; // Auto-download mode
    m_updateManager->checkPaqetUpdate();
}

bool PaqetController::getAutoDownloadPaqet() const {
    return m_settings->autoDownloadPaqet();
}

void PaqetController::setAutoDownloadPaqet(bool enabled) {
    m_settings->setAutoDownloadPaqet(enabled);
}

bool PaqetController::getAutoCheckUpdates() const {
    return m_settings->autoCheckUpdates();
}

void PaqetController::setAutoCheckUpdates(bool enabled) {
    m_settings->setAutoCheckUpdates(enabled);
}

bool PaqetController::getAutoUpdatePaqetN() const {
    return m_settings->autoUpdatePaqetN();
}

void PaqetController::setAutoUpdatePaqetN(bool enabled) {
    m_settings->setAutoUpdatePaqetN(enabled);
}

void PaqetController::onPaqetUpdateCheckFinished(bool available, const QString &version, const QString &url) {
    m_updateCheckInProgress = false;
    emit updateCheckInProgressChanged();

    if (available) {
        emit paqetUpdateAvailable(version, url);
        // Auto-download ONLY if in auto-download mode AND setting is enabled AND binary is missing
        if (m_autoDownloadMode && m_settings->autoDownloadPaqet() && !isPaqetBinaryAvailable()) {
            m_logBuffer->append(tr("[PaqetN] Downloading paqet %1...").arg(version));
            downloadPaqet(version, url);
        }
    }

    // Reset auto-download mode
    m_autoDownloadMode = false;
}

void PaqetController::onPaqetNUpdateCheckFinished(bool available, const QString &version, const QString &url) {
    m_updateCheckInProgress = false;
    emit updateCheckInProgressChanged();

    if (available) {
        emit paqetNUpdateAvailable(version, url);
    }
}

void PaqetController::onPaqetDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (bytesTotal > 0) {
        m_paqetDownloadProgress = static_cast<int>((bytesReceived * 100) / bytesTotal);
        emit paqetDownloadProgressChanged();
    }
}

void PaqetController::onPaqetNDownloadProgress(qint64 bytesReceived, qint64 bytesTotal) {
    if (bytesTotal > 0) {
        m_paqetnDownloadProgress = static_cast<int>((bytesReceived * 100) / bytesTotal);
        emit paqetnDownloadProgressChanged();
    }
}

void PaqetController::onPaqetDownloadFinished(const QString &path) {
    m_paqetDownloadInProgress = false;
    m_paqetDownloadProgress = 100;
    emit paqetDownloadInProgressChanged();
    emit paqetDownloadProgressChanged();
    emit paqetDownloadComplete(path);

    m_logBuffer->append(tr("[PaqetN] Paqet binary installed successfully at: %1").arg(path));

    // Update runner's binary path
    if (m_runner) {
        m_runner->setPaqetBinaryPath(path);
    }
}

void PaqetController::onPaqetNDownloadFinished() {
    m_paqetnDownloadInProgress = false;
    m_paqetnDownloadProgress = 100;
    emit paqetnDownloadInProgressChanged();
    emit paqetnDownloadProgressChanged();
    emit paqetnDownloadComplete();

    m_logBuffer->append(tr("[PaqetN] Update downloaded, restarting..."));
    // App will close and restart via UpdateManager's self-update mechanism
}

QVariantList PaqetController::detectNetworkAdapters() {
    NetworkInfoDetector detector;
    auto adapters = detector.detectAdapters();

    QVariantList result;
    for (const auto &adapter : adapters) {
        QVariantMap map;
        map.insert(QStringLiteral("name"), adapter.name);
        map.insert(QStringLiteral("guid"), adapter.guid);
        map.insert(QStringLiteral("interfaceName"), adapter.interfaceName);
        map.insert(QStringLiteral("ipv4Address"), adapter.ipv4Address);
        map.insert(QStringLiteral("gatewayIp"), adapter.gatewayIp);
        map.insert(QStringLiteral("gatewayMac"), adapter.gatewayMac);
        map.insert(QStringLiteral("isActive"), adapter.isActive);
        result.append(map);
    }

    return result;
}

QVariantMap PaqetController::getDefaultNetworkAdapter() {
    NetworkInfoDetector detector;
    detector.setLogBuffer(m_logBuffer);  // Enable logging to log viewer
    auto adapter = detector.getDefaultAdapter();

    QVariantMap map;
    map.insert(QStringLiteral("name"), adapter.name);
    map.insert(QStringLiteral("guid"), adapter.guid);
    map.insert(QStringLiteral("interfaceName"), adapter.interfaceName);
    map.insert(QStringLiteral("ipv4Address"), adapter.ipv4Address);
    map.insert(QStringLiteral("gatewayIp"), adapter.gatewayIp);
    map.insert(QStringLiteral("gatewayMac"), adapter.gatewayMac);
    map.insert(QStringLiteral("isActive"), adapter.isActive);

    return map;
}
