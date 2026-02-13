#include "PaqetController.h"
#include "PaqetConfig.h"
#include "ChildProcessJob.h"
#include "LogBuffer.h"
#include "PaqetRunner.h"
#include "LatencyChecker.h"
#include "UpdateManager.h"
#include "TunManager.h"
#include "SystemProxyManager.h"
#include "TunAssetsManager.h"
#include "HttpToSocksProxy.h"
#include "NetworkInfoDetector.h"
#include <QGuiApplication>
#include <QClipboard>
#include <QFile>
#include <QTimer>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QDateTime>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shellapi.h>
#else
#include <unistd.h>
#endif

PaqetController::PaqetController(QObject *parent) : QObject(parent) {
    ChildProcessJob::init();
    m_repo = new ConfigRepository(this);
    m_settings = new SettingsRepository(this);
    m_configList = new ConfigListModel(this);
    m_logBuffer = new LogBuffer(this);
    m_runner = new PaqetRunner(m_logBuffer, this);
    m_latencyChecker = new LatencyChecker(this);
    m_updateManager = new UpdateManager(this);
    m_tunManager = new TunManager(m_logBuffer, this);
    m_systemProxyManager = new SystemProxyManager(m_logBuffer, this);
    m_tunAssetsManager = new TunAssetsManager(m_logBuffer, m_tunManager, this);
    m_httpProxy = new HttpToSocksProxy(this);
    m_httpProxy->setLogBuffer(m_logBuffer);

    connect(m_repo, &ConfigRepository::configsChanged, this, &PaqetController::reloadConfigList);
    connect(m_tunManager, &TunManager::runningChanged, this, &PaqetController::tunRunningChanged);
    connect(m_systemProxyManager, &SystemProxyManager::enabledChanged, this, &PaqetController::systemProxyEnabledChanged);

    // Connect TunAssetsManager signals
    connect(m_tunAssetsManager, &TunAssetsManager::tunAssetsMissingPrompt, this, &PaqetController::tunAssetsMissingPrompt);
    connect(m_tunAssetsManager, &TunAssetsManager::tunAssetsDownloadStarted, this, [this] {
        m_tunAssetsDownloadInProgress = true;
        m_tunAssetsDownloadProgress = 0;
        m_downloadFailed = false;
        m_downloadFailedMessage.clear();
        emit tunAssetsDownloadInProgressChanged();
        emit tunAssetsDownloadProgressChanged();
        emit downloadFailedChanged();
        emit downloadFailedMessageChanged();
    });
    connect(m_tunAssetsManager, &TunAssetsManager::tunAssetsDownloadProgress, this, [this](int percent) {
        m_tunAssetsDownloadProgress = percent;
        emit tunAssetsDownloadProgressChanged();
    });
    connect(m_tunAssetsManager, &TunAssetsManager::tunAssetsDownloadFinished, this, [this] {
        m_tunAssetsDownloadInProgress = false;
        m_tunAssetsDownloadProgress = 100;
        emit tunAssetsDownloadInProgressChanged();
        emit tunAssetsDownloadProgressChanged();
    });
    connect(m_tunAssetsManager, &TunAssetsManager::tunAssetsDownloadFailed, this, [this](const QString &error) {
        m_tunAssetsDownloadInProgress = false;
        m_downloadFailed = true;
        m_downloadFailedMessage = error;
        emit tunAssetsDownloadInProgressChanged();
        emit downloadFailedChanged();
        emit downloadFailedMessageChanged();
    });

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
        m_paqetUpdateCheckInProgress = true;
        m_downloadFailed = false;
        m_downloadFailedMessage.clear();
        emit updateCheckInProgressChanged();
        emit paqetUpdateCheckInProgressChanged();
        emit downloadFailedChanged();
        emit downloadFailedMessageChanged();
    });
    connect(m_updateManager, &UpdateManager::paqetUpdateCheckFinished, this, &PaqetController::onPaqetUpdateCheckFinished);
    connect(m_updateManager, &UpdateManager::paqetUpdateCheckFailed, this, [this](const QString &error) {
        m_updateCheckInProgress = false;
        m_paqetUpdateCheckInProgress = false;
        m_updateStatusMessage = error;
        m_downloadFailed = true;
        m_downloadFailedMessage = error;
        emit updateCheckInProgressChanged();
        emit paqetUpdateCheckInProgressChanged();
        emit updateStatusMessageChanged();
        emit downloadFailedChanged();
        emit downloadFailedMessageChanged();
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
        m_downloadFailed = false;
        m_downloadFailedMessage.clear();
        emit paqetDownloadInProgressChanged();
        emit paqetDownloadProgressChanged();
        emit downloadFailedChanged();
        emit downloadFailedMessageChanged();
    });
    connect(m_updateManager, &UpdateManager::paqetDownloadProgress, this, &PaqetController::onPaqetDownloadProgress);
    connect(m_updateManager, &UpdateManager::paqetDownloadFinished, this, &PaqetController::onPaqetDownloadFinished);
    connect(m_updateManager, &UpdateManager::paqetDownloadFailed, this, [this](const QString &error) {
        m_paqetDownloadInProgress = false;
        m_updateStatusMessage = error;
        m_downloadFailed = true;
        m_downloadFailedMessage = error;
        emit paqetDownloadInProgressChanged();
        emit updateStatusMessageChanged();
        emit downloadFailedChanged();
        emit downloadFailedMessageChanged();
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

    // Prompt user to download paqet if missing (when auto-download setting is on)
    QTimer::singleShot(500, this, [this] {
        if (m_settings->autoDownloadPaqet()) {
            if (!m_updateManager->isPaqetBinaryAvailable(m_settings->paqetBinaryPath())) {
                emit paqetBinaryMissingPrompt();
            }
        }
    });

    // Auto-start with last active profile when app starts
    QTimer::singleShot(800, this, [this] {
        if (!isRunning() && !m_selectedConfigId.isEmpty())
            connectToSelected();
    });

    // Ensure cleanup happens when app is about to quit
    connect(QCoreApplication::instance(), &QCoreApplication::aboutToQuit, this, &PaqetController::cleanup);
}

PaqetController::~PaqetController() {
    cleanup();
}

void PaqetController::cleanup() {
    // Prevent multiple cleanups
    static bool cleanedUp = false;
    if (cleanedUp) return;
    cleanedUp = true;

    // Stop network monitoring
    stopNetworkMonitoring();

    if (m_logBuffer)
        m_logBuffer->append(QStringLiteral("[PaqetN] Application closing, cleaning up..."));

    // Stop TUN manager first
    if (m_tunManager && m_tunManager->isRunning()) {
        if (m_logBuffer)
            m_logBuffer->append(QStringLiteral("[PaqetN] Stopping TUN..."));
        m_tunManager->stopBlocking();
    }

    // Disable system proxy
    if (m_systemProxyManager && m_systemProxyManager->isEnabled()) {
        if (m_logBuffer)
            m_logBuffer->append(QStringLiteral("[PaqetN] Disabling system proxy..."));
        m_systemProxyManager->disable();
    }

    // Stop HTTP proxy
    if (m_httpProxy && m_httpProxy->isRunning()) {
        if (m_logBuffer)
            m_logBuffer->append(QStringLiteral("[PaqetN] Stopping HTTP proxy..."));
        m_httpProxy->stop();
    }

    // Stop paqet runner last
    if (m_runner && m_runner->isRunning()) {
        if (m_logBuffer)
            m_logBuffer->append(QStringLiteral("[PaqetN] Stopping paqet..."));
        m_runner->stopBlocking();
    }

    if (m_logBuffer)
        m_logBuffer->append(QStringLiteral("[PaqetN] Cleanup complete."));
}

void PaqetController::setSelectedConfigId(const QString &id) {
    if (m_selectedConfigId == id) return;
    bool wasRunning = isRunning();
    m_selectedConfigId = id;
    m_repo->setLastSelectedId(id);
    emit selectedConfigIdChanged();

    // Check if paqet binary exists and prompt user to download if missing
    QString binaryPath = m_settings->paqetBinaryPath();
    if (!m_updateManager || !m_updateManager->isPaqetBinaryAvailable(binaryPath)) {
        m_logBuffer->append(tr("[PaqetN] ERROR: Paqet binary not found at: %1").arg(binaryPath));
        m_logBuffer->append(tr("[PaqetN] Please download it from the Updates page."));
        emit paqetBinaryMissing();
        return;
    }

    // When user switches profile while connected, restart paqet with the new profile
    if (wasRunning) {
        disconnectAsync([this]() { connectToSelected(); });
    }
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
        // socksListen is now determined by settings (port + allowLocalLan), not stored per-config
        QString bindAddr = m_settings->allowLocalLan() ? QStringLiteral("0.0.0.0") : QStringLiteral("127.0.0.1");
        c.socksListen = QStringLiteral("%1:%2").arg(bindAddr).arg(m_settings->socksPort());
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
        // Notify that selected config data changed so UI refreshes
        if (c.id == m_selectedConfigId)
            emit selectedConfigIdChanged();
        // Restart if updated config is the currently connected profile
        if (c.id == m_connectedConfigId && isRunning())
            restart();
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
    // socksListen is now determined by settings (port + allowLocalLan), not stored per-config
    QString bindAddr = m_settings->allowLocalLan() ? QStringLiteral("0.0.0.0") : QStringLiteral("127.0.0.1");
    c.socksListen = QStringLiteral("%1:%2").arg(bindAddr).arg(m_settings->socksPort());
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
    
    // Check if user has a preferred network interface selected
    QString selectedGuid = m_settings->selectedNetworkInterface();
    if (!selectedGuid.isEmpty()) {
        m_logBuffer->append(tr("[PaqetN] Using user-selected network adapter..."));
    } else {
        m_logBuffer->append(tr("[PaqetN] Auto-detecting network adapter..."));
    }

    // Use port from settings and determine bind address based on allowLocalLan
    int socksPort = m_settings->socksPort();
    bool allowLocalLan = m_settings->allowLocalLan();
    QString bindAddr = allowLocalLan ? QStringLiteral("0.0.0.0") : QStringLiteral("127.0.0.1");
    c.socksListen = QStringLiteral("%1:%2").arg(bindAddr).arg(socksPort);
    
    if (allowLocalLan) {
        m_logBuffer->append(tr("[PaqetN] Allow Local LAN enabled, binding to 0.0.0.0:%1").arg(socksPort));
    }

    // Run network detection off the UI thread to avoid freezing (PowerShell/ipconfig can take seconds)
    QString logLevel = m_settings->logLevel();
    QFutureWatcher<NetworkAdapterInfo> *watcher = new QFutureWatcher<NetworkAdapterInfo>(this);
    connect(watcher, &QFutureWatcher<NetworkAdapterInfo>::finished, this, [this, watcher, c]() mutable {
        qDebug() << "[PaqetController] Network future finished: slot entered";
        NetworkAdapterInfo adapter = watcher->result();
        qDebug() << "[PaqetController] Got result, adapter.name=" << adapter.name;
        QTimer::singleShot(0, this, [watcher]() {
            qDebug() << "[PaqetController] Deferred: about to watcher->deleteLater()";
            watcher->deleteLater();
            qDebug() << "[PaqetController] Deferred: watcher->deleteLater() returned";
        });

        if (!adapter.name.isEmpty() && !adapter.ipv4Address.isEmpty()) {
            c.guid = adapter.guid;
#ifdef Q_OS_WIN
            c.networkInterface = adapter.name;
#else
            c.networkInterface = adapter.interfaceName.isEmpty() ? adapter.name : adapter.interfaceName;
#endif
            c.ipv4Addr = adapter.ipv4Address;
            c.routerMac = adapter.gatewayMac.isEmpty() ? QStringLiteral("00:00:00:00:00:00") : adapter.gatewayMac;
            m_logBuffer->append(tr("[PaqetN] Network adapter detected: %1, IP: %2, Gateway: %3")
                .arg(adapter.name, adapter.ipv4Address, adapter.gatewayIp));
        } else {
            m_logBuffer->append(tr("[PaqetN] WARNING: Could not detect network adapter, using defaults"));
#ifdef Q_OS_WIN
            c.guid = QString();
            c.networkInterface = QString();
#else
            c.networkInterface = QStringLiteral("lo");
            c.guid = QString();
#endif
            c.ipv4Addr = QStringLiteral("127.0.0.1:0");
            c.routerMac = QStringLiteral("00:00:00:00:00:00");
        }

        // Continue connection on main thread (binary check, start runner, etc.)
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

        const QString mode = m_settings->proxyMode();
        if (mode == QLatin1String("tun") && !m_tunAssetsManager->isTunAssetsAvailable()) {
            m_logBuffer->append(tr("[PaqetN] TUN mode requires hev-socks5-tunnel (and on Windows, wintun.dll). They were not found."));
            emit tunAssetsMissingPrompt();
            return;
        }

#ifdef Q_OS_WIN
        // TUN mode requires administrator privileges on Windows
        if (mode == QLatin1String("tun") && !isRunningAsAdmin()) {
            m_logBuffer->append(tr("[PaqetN] TUN mode requires administrator privileges."));
            emit adminPrivilegeRequired();
            return;
        }
#endif

        m_logBuffer->append(tr("[PaqetN] Starting paqet with log level: %1").arg(m_settings->logLevel()));

        // start() is non-blocking; we get started() or startFailed() when the process is up or failed
        struct StartConnections { QMetaObject::Connection started; QMetaObject::Connection failed; };
        StartConnections *conns = new StartConnections();
        conns->started = connect(m_runner, &PaqetRunner::started, this, [this, c, mode, conns]() {
            QObject::disconnect(conns->started);
            QObject::disconnect(conns->failed);
            delete conns;
            m_connectedConfigId = c.id;
            m_connectionEstablishedAt = QDateTime::currentMSecsSinceEpoch();
            m_logBuffer->append(tr("[PaqetN] Connection initiated successfully"));
            if (mode == QLatin1String("tun")) {
                m_logBuffer->append(tr("[PaqetN] Starting TUN mode..."));
                m_tunManager->setTunBinaryPath(m_settings->tunBinaryPath());
                if (!m_tunManager->start(c.socksPort(), c.serverAddr)) {
                    m_logBuffer->append(tr("[PaqetN] WARNING: TUN mode failed to start, SOCKS5 proxy is still active"));
                }
            } else if (mode == QLatin1String("system")) {
                // Start HTTP-to-SOCKS proxy first (HTTP port = SOCKS port + 1)
                quint16 socksPort = c.socksPort();
                quint16 httpPort = socksPort + 1;
                
                if (m_httpProxy && m_httpProxy->start(httpPort, QStringLiteral("127.0.0.1"), socksPort)) {
                    m_logBuffer->append(tr("[PaqetN] HTTP proxy started on port %1").arg(httpPort));
                    
                    // Now set system proxy to use our HTTP proxy
                    m_logBuffer->append(tr("[PaqetN] Setting system proxy..."));
                    if (!m_systemProxyManager->enable(httpPort)) {
                        m_logBuffer->append(tr("[PaqetN] WARNING: System proxy failed, HTTP proxy is still available on port %1").arg(httpPort));
                    }
                } else {
                    m_logBuffer->append(tr("[PaqetN] WARNING: HTTP proxy failed to start, SOCKS5 proxy is still active on port %1").arg(socksPort));
                }
            }
        });
        conns->failed = connect(m_runner, &PaqetRunner::startFailed, this, [this, conns](const QString &err) {
            QObject::disconnect(conns->started);
            QObject::disconnect(conns->failed);
            delete conns;
            m_logBuffer->append(tr("[PaqetN] ERROR: Failed to start paqet process: %1").arg(err));
        });
        m_runner->start(c, m_settings->logLevel());
        qDebug() << "[PaqetController] Network-finished lambda done, m_runner->start() called";
    });

    qDebug() << "[PaqetController] Starting QtConcurrent::run for network detection";
    QFuture<NetworkAdapterInfo> future = QtConcurrent::run([logLevel, selectedGuid]() {
        NetworkInfoDetector detector;
        detector.setLogBuffer(nullptr);  // Do not log from worker thread (LogBuffer not thread-safe)
        detector.setLogLevel(logLevel);
        if (!selectedGuid.isEmpty()) {
            return detector.getAdapterByGuid(selectedGuid);
        }
        return detector.getDefaultAdapter();
    });
    watcher->setFuture(future);
}

void PaqetController::restart() {
    if (!isRunning()) {
        connectToSelected();
        return;
    }
    disconnectAsync([this]() { connectToSelected(); });
}

void PaqetController::disconnectAsync(const std::function<void()> &callback) {
    if (m_systemProxyManager && m_systemProxyManager->isEnabled()) {
        m_logBuffer->append(tr("[PaqetN] Restoring system proxy..."));
        m_systemProxyManager->disable();
    }
    if (m_httpProxy && m_httpProxy->isRunning()) {
        m_logBuffer->append(tr("[PaqetN] Stopping HTTP proxy..."));
        m_httpProxy->stop();
    }
    const bool runnerWasRunning = m_runner && m_runner->isRunning();
    const bool tunWasRunning = m_tunManager && m_tunManager->isRunning();
    if (tunWasRunning) {
        m_logBuffer->append(tr("[PaqetN] Stopping TUN mode..."));
        m_tunManager->stop();
    }
    if (runnerWasRunning)
        m_runner->stop();

    int pending = (runnerWasRunning ? 1 : 0) + (tunWasRunning ? 1 : 0);
    if (pending == 0) {
        m_connectedConfigId.clear();
        m_connectionEstablishedAt = 0;
        m_latencyMs = -1;
        emit latencyMsChanged();
        if (callback) callback();
        return;
    }

    struct State { int pending; std::function<void()> cb; QMetaObject::Connection c1; QMetaObject::Connection c2; };
    State *state = new State{pending, callback, {}, {}};
    auto onStopped = [this, state]() {
        state->pending--;
        if (state->pending > 0) return;
        QObject::disconnect(state->c1);
        QObject::disconnect(state->c2);
        m_connectedConfigId.clear();
        m_connectionEstablishedAt = 0;
        m_latencyMs = -1;
        emit latencyMsChanged();
        if (state->cb) state->cb();
        delete state;
    };
    if (runnerWasRunning)
        state->c1 = connect(m_runner, &PaqetRunner::stopped, this, onStopped);
    if (tunWasRunning)
        state->c2 = connect(m_tunManager, &TunManager::stopped, this, onStopped);
}

void PaqetController::disconnect() {
    disconnectAsync(nullptr);
}

void PaqetController::testLatency() {
    PaqetConfig c = selectedConfig();
    if (c.id.isEmpty()) return;
    auto doCheck = [this]() {
        PaqetConfig cfg = selectedConfig();
        if (cfg.id.isEmpty()) return;
        m_latencyChecker->check(cfg.socksPort(), m_settings->connectionCheckUrl());
    };
    // If we just connected (e.g. after profile switch), wait for SOCKS to be ready
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const bool recentlyConnected = isRunning() && m_connectionEstablishedAt > 0
        && (now - m_connectionEstablishedAt) < 3000;
    if (recentlyConnected) {
        m_latencyTesting = true;
        emit latencyTestingChanged();
        QTimer::singleShot(2000, this, doCheck);
    } else {
        doCheck();
    }
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
void PaqetController::setSocksPort(int port) {
    int currentPort = m_settings->socksPort();
    if (currentPort == port) return;  // No change

    m_settings->setSocksPort(port);

    // Restart paqet if running to apply new port
    if (isRunning()) {
        m_logBuffer->append(tr("[PaqetN] SOCKS port changed, restarting..."));
        restart();
    }
}
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

void PaqetController::clearDownloadFailed() {
    if (!m_downloadFailed) return;
    m_downloadFailed = false;
    m_downloadFailedMessage.clear();
    emit downloadFailedChanged();
    emit downloadFailedMessageChanged();
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
    m_paqetUpdateCheckInProgress = false;
    emit updateCheckInProgressChanged();
    emit paqetUpdateCheckInProgressChanged();

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

    // Auto-start connection if a profile is selected but not running
    if (!isRunning() && !m_selectedConfigId.isEmpty()) {
        m_logBuffer->append(tr("[PaqetN] Profile selected, starting connection..."));
        connectToSelected();
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

// Proxy mode methods
QString PaqetController::proxyMode() const { return m_settings->proxyMode(); }
bool PaqetController::tunRunning() const { return m_tunManager && m_tunManager->isRunning(); }
bool PaqetController::systemProxyEnabled() const { return m_systemProxyManager && m_systemProxyManager->isEnabled(); }
QString PaqetController::getProxyMode() const { return m_settings->proxyMode(); }
void PaqetController::setProxyMode(const QString &mode) {
    QString oldMode = m_settings->proxyMode();
    if (oldMode == mode) return;

    // When switching to TUN, validate requirements before persisting so that if the user
    // dismisses the download/admin prompt (Later/Cancel), the mode switch stays on the previous mode.
    if (mode == QLatin1String("tun")) {
        if (m_tunAssetsManager && !m_tunAssetsManager->isTunAssetsAvailable()) {
            m_logBuffer->append(tr("[PaqetN] TUN assets not found, prompting download..."));
            emit tunAssetsMissingPrompt();
            emit proxyModeChanged(); // Force UI combo to re-read unchanged proxyMode so it reverts visually
            return;
        }
#ifdef Q_OS_WIN
        if (!isRunningAsAdmin()) {
            m_logBuffer->append(tr("[PaqetN] TUN mode requires administrator privileges."));
            emit adminPrivilegeRequired();
            emit proxyModeChanged(); // Force UI combo to re-read unchanged proxyMode so it reverts visually
            return;
        }
#endif
    }

    m_settings->setProxyMode(mode);
    emit proxyModeChanged();

    // If not running, just save the setting
    if (!isRunning()) return;

    PaqetConfig c = selectedConfig();
    if (c.id.isEmpty()) return;

    // Stop the old proxy mode
    if (oldMode == QLatin1String("system")) {
        if (m_systemProxyManager && m_systemProxyManager->isEnabled()) {
            m_logBuffer->append(tr("[PaqetN] Disabling system proxy..."));
            m_systemProxyManager->disable();
        }
        if (m_httpProxy && m_httpProxy->isRunning()) {
            m_logBuffer->append(tr("[PaqetN] Stopping HTTP proxy..."));
            m_httpProxy->stop();
        }
    } else if (oldMode == QLatin1String("tun") && m_tunManager && m_tunManager->isRunning()) {
        m_logBuffer->append(tr("[PaqetN] Stopping TUN mode..."));
        m_tunManager->stop();
    }

    // Start the new proxy mode
    if (mode == QLatin1String("system")) {
        // Start HTTP-to-SOCKS proxy first (HTTP port = SOCKS port + 1)
        quint16 socksPort = c.socksPort();
        quint16 httpPort = socksPort + 1;
        
        if (m_httpProxy && m_httpProxy->start(httpPort, QStringLiteral("127.0.0.1"), socksPort)) {
            m_logBuffer->append(tr("[PaqetN] HTTP proxy started on port %1").arg(httpPort));
            
            // Now set system proxy to use our HTTP proxy
            m_logBuffer->append(tr("[PaqetN] Setting system proxy..."));
            if (!m_systemProxyManager->enable(httpPort)) {
                m_logBuffer->append(tr("[PaqetN] WARNING: System proxy failed, HTTP proxy is still available on port %1").arg(httpPort));
            }
        } else {
            m_logBuffer->append(tr("[PaqetN] WARNING: HTTP proxy failed to start, SOCKS5 proxy is still active on port %1").arg(socksPort));
        }
    } else if (mode == QLatin1String("tun")) {
        m_logBuffer->append(tr("[PaqetN] Starting TUN mode..."));
        m_tunManager->setTunBinaryPath(m_settings->tunBinaryPath());
        if (!m_tunManager->start(c.socksPort(), c.serverAddr)) {
            m_logBuffer->append(tr("[PaqetN] WARNING: TUN mode failed to start, SOCKS5 proxy is still active"));
        }
    }
}
QStringList PaqetController::getProxyModes() const { return SettingsRepository::proxyModes(); }
QString PaqetController::getTunBinaryPath() const { return m_settings->tunBinaryPath(); }
void PaqetController::setTunBinaryPath(const QString &path) { m_settings->setTunBinaryPath(path); }
bool PaqetController::isTunAssetsAvailable() const { return m_tunAssetsManager && m_tunAssetsManager->isTunAssetsAvailable(); }
void PaqetController::autoDownloadTunAssetsIfMissing() { if (m_tunAssetsManager) m_tunAssetsManager->downloadTunAssets(); }

bool PaqetController::getStartOnBoot() const { return m_settings->startOnBoot(); }
void PaqetController::setStartOnBoot(bool enabled) {
    m_settings->setStartOnBoot(enabled);
#ifdef Q_OS_WIN
    // Update Windows registry for startup
    QSettings bootSettings(QStringLiteral("HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Run"),
                           QSettings::NativeFormat);
    if (enabled) {
        QString appPath = QCoreApplication::applicationFilePath();
        bootSettings.setValue(QStringLiteral("paqetN"), QDir::toNativeSeparators(appPath));
    } else {
        bootSettings.remove(QStringLiteral("paqetN"));
    }
#endif
}
bool PaqetController::getAutoHideOnStartup() const { return m_settings->autoHideOnStartup(); }
void PaqetController::setAutoHideOnStartup(bool enabled) { m_settings->setAutoHideOnStartup(enabled); }
bool PaqetController::getCloseToTray() const { return m_settings->closeToTray(); }
void PaqetController::setCloseToTray(bool enabled) { m_settings->setCloseToTray(enabled); }

void PaqetController::requestQuit() {
    // Schedule quit in C++ so it runs in the main event loop after the native
    // tray menu has closed (QML Timer / Qt.quit often never run on Windows otherwise).
    QTimer::singleShot(300, []() { QCoreApplication::quit(); });
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

// Runs in a worker thread to avoid blocking the UI (PowerShell + ipconfig + arp are slow).
static QVariantList fetchAcceptableNetworkAdaptersInThread() {
    NetworkInfoDetector detector;
    auto adapters = detector.getAcceptableAdapters();
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

QVariantList PaqetController::getAcceptableNetworkAdapters() {
    // Return cached list when valid to avoid blocking the UI (e.g. when QML refreshes after networkAdaptersChanged).
    if (m_networkAdaptersCacheValid) {
        return m_cachedAdapters;
    }
    // First load or cache invalid: run synchronously and cache result.
    m_cachedAdapters = fetchAcceptableNetworkAdaptersInThread();
    m_networkAdaptersCacheValid = true;
    return m_cachedAdapters;
}

QString PaqetController::getSelectedNetworkInterface() const {
    return m_settings->selectedNetworkInterface();
}

void PaqetController::setSelectedNetworkInterface(const QString &guid) {
    m_settings->setSelectedNetworkInterface(guid);
}

bool PaqetController::getAllowLocalLan() const {
    return m_settings->allowLocalLan();
}

void PaqetController::setAllowLocalLan(bool enabled) {
    m_settings->setAllowLocalLan(enabled);
}

void PaqetController::startNetworkMonitoring() {
    if (!m_networkMonitorTimer) {
        m_networkMonitorTimer = new QTimer(this);
        m_networkMonitorTimer->setInterval(5000);  // Check every 5 seconds
        connect(m_networkMonitorTimer, &QTimer::timeout, this, &PaqetController::checkNetworkChanges);
    }
    if (!m_networkMonitorWatcher) {
        m_networkMonitorWatcher = new QFutureWatcher<QVariantList>(this);
        connect(m_networkMonitorWatcher, &QFutureWatcher<QVariantList>::finished, this, &PaqetController::onNetworkMonitorFinished);
    }
    
    // Initialize from cache or one synchronous run (only blocks once at startup if cache empty)
    QVariantList adapters = getAcceptableNetworkAdapters();
    m_lastAdapterGuids.clear();
    for (const QVariant &v : adapters) {
        m_lastAdapterGuids.append(v.toMap().value(QStringLiteral("guid")).toString());
    }
    
    m_networkMonitorTimer->start();
}

void PaqetController::stopNetworkMonitoring() {
    if (m_networkMonitorTimer) {
        m_networkMonitorTimer->stop();
    }
}

void PaqetController::checkNetworkChanges() {
    // Run heavy detection (PowerShell + ipconfig + arp) in background to avoid blocking the UI.
    if (m_networkMonitorWatcher && m_networkMonitorWatcher->isRunning()) {
        return;  // Skip this tick if previous run still in progress
    }
    m_networkMonitorWatcher->setFuture(QtConcurrent::run(fetchAcceptableNetworkAdaptersInThread));
}

void PaqetController::onNetworkMonitorFinished() {
    if (!m_networkMonitorWatcher) return;
    QVariantList adapters = m_networkMonitorWatcher->result();
    QStringList currentGuids;
    for (const QVariant &v : adapters) {
        currentGuids.append(v.toMap().value(QStringLiteral("guid")).toString());
    }
    bool changed = (currentGuids.size() != m_lastAdapterGuids.size());
    if (!changed) {
        for (const QString &guid : currentGuids) {
            if (!m_lastAdapterGuids.contains(guid)) {
                changed = true;
                break;
            }
        }
    }
    if (changed) {
        m_lastAdapterGuids = currentGuids;
        m_cachedAdapters = adapters;
        m_networkAdaptersCacheValid = true;
        emit networkAdaptersChanged();
    } else {
        m_cachedAdapters = adapters;
        m_networkAdaptersCacheValid = true;
    }
}

bool PaqetController::isRunningAsAdmin() const
{
#ifdef Q_OS_WIN
    BOOL isAdmin = FALSE;
    PSID adminGroup = NULL;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;

    if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
                                  DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup)) {
        CheckTokenMembership(NULL, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }
    return isAdmin == TRUE;
#else
    // On non-Windows, check if running as root
    return geteuid() == 0;
#endif
}

// Passed to the elevated process so it retries the single-instance lock while the current process exits
const char PaqetController::kElevatedRestartArg[] = "--elevated-restart";

void PaqetController::restartAsAdmin()
{
#ifdef Q_OS_WIN
    QString exePath = QCoreApplication::applicationFilePath();
    QStringList args = QCoreApplication::arguments();
    args.removeFirst(); // drop executable path
    if (!args.contains(QLatin1String(kElevatedRestartArg)))
        args.append(QLatin1String(kElevatedRestartArg));
    QString params = args.join(QLatin1Char(' '));

    if (m_logBuffer)
        m_logBuffer->append(tr("[PaqetN] Restarting with administrator privileges..."));

    SHELLEXECUTEINFOW sei = { sizeof(sei) };
    sei.lpVerb = L"runas";
    sei.lpFile = reinterpret_cast<LPCWSTR>(exePath.utf16());
    sei.lpParameters = reinterpret_cast<LPCWSTR>(params.utf16());
    sei.nShow = SW_SHOWNORMAL;

    if (ShellExecuteExW(&sei)) {
        // Successfully started elevated process, quit current instance
        QCoreApplication::quit();
    } else {
        DWORD error = GetLastError();
        if (error == ERROR_CANCELLED) {
            if (m_logBuffer)
                m_logBuffer->append(tr("[PaqetN] User cancelled elevation request"));
        } else {
            if (m_logBuffer)
                m_logBuffer->append(tr("[PaqetN] Failed to restart as administrator (error: %1)").arg(error));
        }
    }
#else
    if (m_logBuffer)
        m_logBuffer->append(tr("[PaqetN] Admin restart not supported on this platform"));
#endif
}
