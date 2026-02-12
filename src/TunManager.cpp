#include "TunManager.h"
#include "LogBuffer.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QTimer>
#include <QThread>

#ifdef Q_OS_WIN
#include <winsock2.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")
#endif

TunManager::TunManager(LogBuffer *logBuffer, QObject *parent)
    : QObject(parent), m_logBuffer(logBuffer) {
    m_process = new QProcess(this);
    connect(m_process, &QProcess::stateChanged, this, &TunManager::onProcessStateChanged);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &TunManager::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &TunManager::onReadyReadStandardError);
    connect(m_process, &QProcess::errorOccurred, this, &TunManager::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TunManager::onProcessFinished);
}

#ifdef Q_OS_WIN
int TunManager::getTunInterfaceIndex() const {
    // Get interface index for the TUN adapter (tun0)
    ULONG bufLen = 15000;
    PIP_ADAPTER_ADDRESSES pAddresses = nullptr;
    PIP_ADAPTER_ADDRESSES pCurrAddresses = nullptr;

    pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(bufLen);
    if (!pAddresses) return -1;

    DWORD dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &bufLen);
    if (dwRetVal == ERROR_BUFFER_OVERFLOW) {
        free(pAddresses);
        pAddresses = (IP_ADAPTER_ADDRESSES *)malloc(bufLen);
        if (!pAddresses) return -1;
        dwRetVal = GetAdaptersAddresses(AF_INET, GAA_FLAG_INCLUDE_PREFIX, nullptr, pAddresses, &bufLen);
    }

    if (dwRetVal != NO_ERROR) {
        free(pAddresses);
        return -1;
    }

    int ifIndex = -1;
    pCurrAddresses = pAddresses;
    while (pCurrAddresses) {
        QString friendlyName = QString::fromWCharArray(pCurrAddresses->FriendlyName);
        QString description = QString::fromWCharArray(pCurrAddresses->Description);
        int currentIdx = static_cast<int>(pCurrAddresses->IfIndex);
        
        // Log all wintun-related adapters for debugging
        if (m_logBuffer && (description.contains(QStringLiteral("wintun"), Qt::CaseInsensitive) ||
                           friendlyName.contains(QStringLiteral("tun"), Qt::CaseInsensitive))) {
            m_logBuffer->append(QStringLiteral("[TUN] Found adapter: %1 (idx=%2, desc=%3)")
                .arg(friendlyName).arg(currentIdx).arg(description));
        }
        
        // Look for our TUN adapter - must be named exactly "tun0"
        // We check for exact match to avoid finding old/stale wintun adapters
        if (friendlyName == QStringLiteral("tun0")) {
            ifIndex = currentIdx;
            
            // Check if interface has an IP address assigned (should be 172.20.0.1)
            PIP_ADAPTER_UNICAST_ADDRESS pUnicast = pCurrAddresses->FirstUnicastAddress;
            while (pUnicast) {
                if (pUnicast->Address.lpSockaddr->sa_family == AF_INET) {
                    sockaddr_in *sin = (sockaddr_in *)pUnicast->Address.lpSockaddr;
                    struct in_addr addr = sin->sin_addr;
                    char *ipStr = inet_ntoa(addr);
                    if (ipStr) {
                        QString ipQStr = QString::fromLatin1(ipStr);
                        
                        if (m_logBuffer && ipQStr.startsWith(QStringLiteral("198.18."))) {
                            m_logBuffer->append(QStringLiteral("[TUN] Found interface %1 with IP %2").arg(friendlyName, ipQStr));
                        }
                    }
                }
                pUnicast = pUnicast->Next;
            }
            
            // Interface found - return it
            break;
        }
        pCurrAddresses = pCurrAddresses->Next;
    }

    free(pAddresses);
    return ifIndex;
}

bool TunManager::waitForTunInterface(int timeoutMs) {
    // Wait for the TUN interface to appear
    int elapsed = 0;
    const int pollInterval = 200;
    
    while (elapsed < timeoutMs) {
        int ifIndex = getTunInterfaceIndex();
        if (ifIndex > 0) {
            m_tunInterfaceIndex = ifIndex;
            if (m_logBuffer)
                m_logBuffer->append(QStringLiteral("[TUN] Interface detected, index: %1").arg(ifIndex));
            return true;
        }
        QThread::msleep(pollInterval);
        elapsed += pollInterval;
    }
    
    if (m_logBuffer)
        m_logBuffer->append(QStringLiteral("[TUN] WARNING: TUN interface not detected within timeout"));
    return false;
}
#endif

QString TunManager::resolveTunBinary() const {
    if (!m_customBinaryPath.isEmpty()) {
        QFileInfo fi(m_customBinaryPath);
        if (fi.isExecutable()) return fi.absoluteFilePath();
    }
    const QString coresDir = QCoreApplication::applicationDirPath() + QLatin1String("/cores");
#ifdef Q_OS_WIN
    const QString exe = coresDir + QLatin1String("/hev-socks5-tunnel.exe");
#else
    const QString exe = coresDir + QLatin1String("/hev-socks5-tunnel");
#endif
    if (QFileInfo::exists(exe)) return exe;
    return QStringLiteral("hev-socks5-tunnel");
}

QString TunManager::generateConfig(int socksPort) {
    // hev-socks5-tunnel YAML configuration
    // Reference: https://github.com/heiher/hev-socks5-tunnel/blob/master/conf/main.yml
    QString yaml;
    yaml += QStringLiteral("tunnel:\n");
    yaml += QStringLiteral("  name: tun0\n");
    yaml += QStringLiteral("  mtu: 1500\n");
    yaml += QStringLiteral("  multi-queue: false\n");
    yaml += QStringLiteral("  ipv4: 172.20.0.1\n");  // Use 172.20.x.x range to avoid conflicts
    yaml += QStringLiteral("\n");
    yaml += QStringLiteral("socks5:\n");
    yaml += QStringLiteral("  port: %1\n").arg(socksPort);
    yaml += QStringLiteral("  address: 127.0.0.1\n");
    yaml += QStringLiteral("  udp: 'udp'\n");
    yaml += QStringLiteral("\n");
    yaml += QStringLiteral("misc:\n");
    yaml += QStringLiteral("  log-level: debug\n");
    return yaml;
}

bool TunManager::start(int socksPort, const QString &serverAddr) {
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Stopping any existing TUN process..."));
    stop();

    m_serverAddr = serverAddr;
    m_tunInterfaceIndex = -1;

    // Generate config
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Generating hev-socks5-tunnel config..."));
    const QString yaml = generateConfig(socksPort);

    if (m_logBuffer) {
        m_logBuffer->append(QStringLiteral("[TUN] Generated config:"));
        for (const QString &line : yaml.split(QLatin1Char('\n'))) {
            m_logBuffer->append(QStringLiteral("  ") + line);
        }
    }

    // Write config to temp file
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QLatin1String("/paqetN");
    QDir().mkpath(dir);
    QFile f(dir + QLatin1String("/tun_config.yaml"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] ERROR: Failed to create config file: ") + f.fileName());
        return false;
    }
    f.write(yaml.toUtf8());
    f.close();
    m_configPath = f.fileName();

    // Resolve binary
    const QString binary = resolveTunBinary();
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Resolved binary: ") + binary);

    QFileInfo binaryInfo(binary);
    if (!binaryInfo.exists()) {
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] ERROR: Binary does not exist: ") + binary);
        return false;
    }

    // First, add route for server IP via original gateway (before TUN is up)
    // This ensures we don't lose connection to the server when TUN takes over routing
    if (!setupServerRoute(serverAddr)) {
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] WARNING: Server route setup had issues"));
    }

    // Launch process
    m_process->setWorkingDirectory(dir);
    m_process->setProgram(binary);
    m_process->setArguments({m_configPath});

    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Starting: ") + binary + QLatin1Char(' ') + m_configPath);
    m_process->start(QProcess::ReadOnly);

    const bool ok = m_process->waitForStarted(5000);
    if (!ok) {
        if (m_logBuffer) {
            m_logBuffer->append(QStringLiteral("[TUN] ERROR: Failed to start process"));
            m_logBuffer->append(QStringLiteral("[TUN] Error: ") + m_process->errorString());
        }
        cleanupRoutes();
        return false;
    }

    if (m_logBuffer)
        m_logBuffer->append(QStringLiteral("[TUN] Process started (PID: %1)").arg(m_process->processId()));

#ifdef Q_OS_WIN
    // Wait for TUN interface to be created by hev-socks5-tunnel
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Waiting for TUN interface to be created..."));
    if (!waitForTunInterface(10000)) {
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] ERROR: TUN interface was not created"));
        stop();
        return false;
    }
    
    // Give hev-socks5-tunnel additional time to fully configure the interface
    // (assign IP address, set up internal state, etc.)
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Waiting for interface initialization to complete..."));
    QThread::msleep(3000);
    
    // Check if process is still running
    if (m_process->state() != QProcess::Running) {
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] ERROR: hev-socks5-tunnel process died during initialization"));
        return false;
    }
#else
    // Give Linux/macOS a moment to create the interface
    QThread::msleep(500);
#endif

    // Now setup the TUN routes (after interface is ready)
    if (!setupTunRoutes()) {
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] WARNING: TUN route setup had issues"));
    }

    return true;
}

void TunManager::stop() {
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        cleanupRoutes();
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Stopped"));
        emit stopped();
        return;
    }
    m_process->terminate();
    QTimer::singleShot(3000, this, [this]() {
        if (m_process && m_process->state() != QProcess::NotRunning)
            m_process->kill();
    });
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Stopping..."));
}

void TunManager::stopBlocking() {
    if (!m_process || m_process->state() == QProcess::NotRunning) {
        cleanupRoutes();
        return;
    }
    m_process->terminate();
    if (!m_process->waitForFinished(3000))
        m_process->kill();
    cleanupRoutes();
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Stopped"));
}

bool TunManager::setupServerRoute(const QString &serverAddr) {
    // Extract server IP (strip port if present)
    QString serverIp = serverAddr;
    int colonIdx = serverIp.lastIndexOf(QLatin1Char(':'));
    if (colonIdx > 0) serverIp = serverIp.left(colonIdx);

    if (m_logBuffer)
        m_logBuffer->append(QStringLiteral("[TUN] Setting up server route for: ") + serverIp);

#ifdef Q_OS_WIN
    // Get current default gateway
    QProcess routeProc;
    routeProc.start(QStringLiteral("cmd"), {QStringLiteral("/c"), QStringLiteral("route print 0.0.0.0")});
    if (routeProc.waitForFinished(5000)) {
        QString output = QString::fromLocal8Bit(routeProc.readAllStandardOutput());
        // Parse default gateway from "route print" output
        // Look for line with 0.0.0.0 destination
        for (const QString &line : output.split(QLatin1Char('\n'))) {
            QString trimmed = line.trimmed();
            if (trimmed.startsWith(QLatin1String("0.0.0.0"))) {
                QStringList parts = trimmed.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
                if (parts.size() >= 3) {
                    m_originalGateway = parts[2]; // Gateway column
                    if (parts.size() >= 4) m_originalInterface = parts[3]; // Interface column
                    break;
                }
            }
        }
    }

    if (m_originalGateway.isEmpty()) {
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] WARNING: Could not detect default gateway"));
        return false;
    }

    if (m_logBuffer)
        m_logBuffer->append(QStringLiteral("[TUN] Original gateway: ") + m_originalGateway);

    // Add route for paqet server via original gateway (so it doesn't go through TUN)
    QProcess::execute(QStringLiteral("route"), {QStringLiteral("add"), serverIp, QStringLiteral("mask"),
        QStringLiteral("255.255.255.255"), m_originalGateway, QStringLiteral("metric"), QStringLiteral("5")});

#else // Linux/macOS
    // Get current default gateway
    QProcess routeProc;
    routeProc.start(QStringLiteral("ip"), {QStringLiteral("route"), QStringLiteral("show"), QStringLiteral("default")});
    if (routeProc.waitForFinished(5000)) {
        QString output = QString::fromUtf8(routeProc.readAllStandardOutput()).trimmed();
        // Parse: "default via 192.168.1.1 dev eth0"
        QStringList parts = output.split(QRegularExpression(QStringLiteral("\\s+")), Qt::SkipEmptyParts);
        int viaIdx = parts.indexOf(QStringLiteral("via"));
        if (viaIdx >= 0 && viaIdx + 1 < parts.size()) {
            m_originalGateway = parts[viaIdx + 1];
        }
        int devIdx = parts.indexOf(QStringLiteral("dev"));
        if (devIdx >= 0 && devIdx + 1 < parts.size()) {
            m_originalInterface = parts[devIdx + 1];
        }
    }

    if (m_originalGateway.isEmpty()) {
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] WARNING: Could not detect default gateway"));
        return false;
    }

    if (m_logBuffer)
        m_logBuffer->append(QStringLiteral("[TUN] Original gateway: %1 dev %2").arg(m_originalGateway, m_originalInterface));

    // Add route for paqet server via original gateway
    QProcess::execute(QStringLiteral("ip"), {QStringLiteral("route"), QStringLiteral("add"),
        serverIp + QStringLiteral("/32"), QStringLiteral("via"), m_originalGateway,
        QStringLiteral("dev"), m_originalInterface});
#endif

    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Server route configured"));
    return true;
}

bool TunManager::setupTunRoutes() {
    if (m_logBuffer)
        m_logBuffer->append(QStringLiteral("[TUN] Setting up TUN routes..."));

#ifdef Q_OS_WIN
    if (m_tunInterfaceIndex <= 0) {
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] ERROR: No TUN interface index available"));
        return false;
    }

    QString ifStr = QString::number(m_tunInterfaceIndex);
    
    if (m_logBuffer)
        m_logBuffer->append(QStringLiteral("[TUN] Adding routes via interface %1").arg(ifStr));

    // Try using netsh which is more reliable on modern Windows
    // netsh interface ipv4 add route <prefix>/<length> <interface> <nexthop> metric=<metric> store=active
    
    // Route first half of IP space (0.0.0.0/1 = 0.0.0.0 - 127.255.255.255) through TUN
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Adding route 0.0.0.0/1 via netsh..."));
    QProcess netshProc1;
    netshProc1.start(QStringLiteral("netsh"), {QStringLiteral("interface"), QStringLiteral("ipv4"), 
        QStringLiteral("add"), QStringLiteral("route"), QStringLiteral("0.0.0.0/1"),
        QStringLiteral("interface=") + ifStr, QStringLiteral("nexthop=172.20.0.1"), 
        QStringLiteral("metric=1"), QStringLiteral("store=active")});
    netshProc1.waitForFinished(5000);
    int exitCode1 = netshProc1.exitCode();
    QString output1 = QString::fromLocal8Bit(netshProc1.readAllStandardOutput());
    QString error1 = QString::fromLocal8Bit(netshProc1.readAllStandardError());
    
    if (exitCode1 != 0) {
        if (m_logBuffer) {
            m_logBuffer->append(QStringLiteral("[TUN] netsh route 0.0.0.0/1 failed (exit: %1), trying route command...").arg(exitCode1));
            if (!error1.isEmpty()) m_logBuffer->append(QStringLiteral("[TUN] Error: %1").arg(error1.trimmed()));
        }
        // Fallback to route command
        QProcess routeProc1;
        routeProc1.start(QStringLiteral("route"), {QStringLiteral("add"), QStringLiteral("0.0.0.0"), QStringLiteral("mask"),
            QStringLiteral("128.0.0.0"), QStringLiteral("172.20.0.1"), QStringLiteral("metric"), QStringLiteral("1"),
            QStringLiteral("IF"), ifStr});
        routeProc1.waitForFinished(3000);
        if (routeProc1.exitCode() == 0) {
            if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Route 0.0.0.0/1 added via route command"));
        } else {
            QString routeErr = QString::fromLocal8Bit(routeProc1.readAllStandardError());
            if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] WARNING: route command also failed: %1").arg(routeErr.trimmed()));
        }
    } else {
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Route 0.0.0.0/1 added successfully via netsh"));
    }

    // Route second half of IP space (128.0.0.0/1 = 128.0.0.0 - 255.255.255.255) through TUN
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Adding route 128.0.0.0/1 via netsh..."));
    QProcess netshProc2;
    netshProc2.start(QStringLiteral("netsh"), {QStringLiteral("interface"), QStringLiteral("ipv4"), 
        QStringLiteral("add"), QStringLiteral("route"), QStringLiteral("128.0.0.0/1"),
        QStringLiteral("interface=") + ifStr, QStringLiteral("nexthop=172.20.0.1"), 
        QStringLiteral("metric=1"), QStringLiteral("store=active")});
    netshProc2.waitForFinished(5000);
    int exitCode2 = netshProc2.exitCode();
    QString output2 = QString::fromLocal8Bit(netshProc2.readAllStandardOutput());
    QString error2 = QString::fromLocal8Bit(netshProc2.readAllStandardError());
    
    if (exitCode2 != 0) {
        if (m_logBuffer) {
            m_logBuffer->append(QStringLiteral("[TUN] netsh route 128.0.0.0/1 failed (exit: %1), trying route command...").arg(exitCode2));
            if (!error2.isEmpty()) m_logBuffer->append(QStringLiteral("[TUN] Error: %1").arg(error2.trimmed()));
        }
        // Fallback to route command
        QProcess routeProc2;
        routeProc2.start(QStringLiteral("route"), {QStringLiteral("add"), QStringLiteral("128.0.0.0"), QStringLiteral("mask"),
            QStringLiteral("128.0.0.0"), QStringLiteral("172.20.0.1"), QStringLiteral("metric"), QStringLiteral("1"),
            QStringLiteral("IF"), ifStr});
        routeProc2.waitForFinished(3000);
        if (routeProc2.exitCode() == 0) {
            if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Route 128.0.0.0/1 added via route command"));
        } else {
            QString routeErr = QString::fromLocal8Bit(routeProc2.readAllStandardError());
            if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] WARNING: route command also failed: %1").arg(routeErr.trimmed()));
        }
    } else {
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Route 128.0.0.0/1 added successfully via netsh"));
    }

    // Print full routing table for diagnosis
    if (m_logBuffer) {
        m_logBuffer->append(QStringLiteral("[TUN] Current routing table:"));
        QProcess routePrintProc;
        routePrintProc.start(QStringLiteral("route"), {QStringLiteral("print")});
        routePrintProc.waitForFinished(5000);
        QString routeOutput = QString::fromLocal8Bit(routePrintProc.readAllStandardOutput());
        
        // Only show IPv4 route table section
        bool inIpv4Section = false;
        for (const QString &line : routeOutput.split(QLatin1Char('\n'))) {
            QString trimmed = line.trimmed();
            if (trimmed.contains(QStringLiteral("IPv4 Route Table"))) {
                inIpv4Section = true;
            }
            if (trimmed.contains(QStringLiteral("IPv6 Route Table"))) {
                inIpv4Section = false;
            }
            if (inIpv4Section && !trimmed.isEmpty()) {
                // Show key routes: default (0.0.0.0) and our routes (128.0.0.0)
                if (trimmed.startsWith(QStringLiteral("0.0.0.0")) ||
                    trimmed.startsWith(QStringLiteral("128.0.0.0")) ||
                    trimmed.contains(QStringLiteral("198.18.")) ||
                    trimmed.contains(QStringLiteral("Network")) ||
                    trimmed.contains(QStringLiteral("Destination"))) {
                    m_logBuffer->append(QStringLiteral("[TUN]   %1").arg(trimmed));
                }
            }
        }
        
        // Also check interface info
        QProcess netshProc;
        netshProc.start(QStringLiteral("netsh"), {QStringLiteral("interface"), QStringLiteral("ipv4"), 
            QStringLiteral("show"), QStringLiteral("interfaces")});
        netshProc.waitForFinished(3000);
        QString netshOutput = QString::fromLocal8Bit(netshProc.readAllStandardOutput());
        m_logBuffer->append(QStringLiteral("[TUN] Network interfaces:"));
        for (const QString &line : netshOutput.split(QLatin1Char('\n'))) {
            QString trimmed = line.trimmed();
            if (trimmed.contains(QStringLiteral("tun"), Qt::CaseInsensitive) ||
                trimmed.contains(QStringLiteral("wintun"), Qt::CaseInsensitive) ||
                trimmed.contains(QStringLiteral("Idx")) ||
                trimmed.contains(QString::number(m_tunInterfaceIndex))) {
                m_logBuffer->append(QStringLiteral("[TUN]   %1").arg(trimmed));
            }
        }
    }

#else // Linux/macOS
    // Route all traffic through TUN
    QProcess::execute(QStringLiteral("ip"), {QStringLiteral("route"), QStringLiteral("add"),
        QStringLiteral("0.0.0.0/1"), QStringLiteral("dev"), QStringLiteral("tun0")});
    QProcess::execute(QStringLiteral("ip"), {QStringLiteral("route"), QStringLiteral("add"),
        QStringLiteral("128.0.0.0/1"), QStringLiteral("dev"), QStringLiteral("tun0")});
#endif

    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] TUN routes configured"));
    return true;
}

void TunManager::cleanupRoutes() {
    if (m_serverAddr.isEmpty()) return;

    QString serverIp = m_serverAddr;
    int colonIdx = serverIp.lastIndexOf(QLatin1Char(':'));
    if (colonIdx > 0) serverIp = serverIp.left(colonIdx);

    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Cleaning up routes..."));

#ifdef Q_OS_WIN
    // Delete server route
    QProcess::execute(QStringLiteral("route"), {QStringLiteral("delete"), serverIp});
    
    // Delete TUN routes (try both netsh and route command to ensure cleanup)
    QString ifStr = QString::number(m_tunInterfaceIndex);
    if (m_tunInterfaceIndex > 0) {
        QProcess::execute(QStringLiteral("netsh"), {QStringLiteral("interface"), QStringLiteral("ipv4"), 
            QStringLiteral("delete"), QStringLiteral("route"), QStringLiteral("0.0.0.0/1"),
            QStringLiteral("interface=") + ifStr, QStringLiteral("store=active")});
        QProcess::execute(QStringLiteral("netsh"), {QStringLiteral("interface"), QStringLiteral("ipv4"), 
            QStringLiteral("delete"), QStringLiteral("route"), QStringLiteral("128.0.0.0/1"),
            QStringLiteral("interface=") + ifStr, QStringLiteral("store=active")});
    }
    // Also try route command for cleanup
    QProcess::execute(QStringLiteral("route"), {QStringLiteral("delete"), QStringLiteral("0.0.0.0"), QStringLiteral("mask"), QStringLiteral("128.0.0.0")});
    QProcess::execute(QStringLiteral("route"), {QStringLiteral("delete"), QStringLiteral("128.0.0.0"), QStringLiteral("mask"), QStringLiteral("128.0.0.0")});
#else
    QProcess::execute(QStringLiteral("ip"), {QStringLiteral("route"), QStringLiteral("del"),
        serverIp + QStringLiteral("/32")});
    QProcess::execute(QStringLiteral("ip"), {QStringLiteral("route"), QStringLiteral("del"),
        QStringLiteral("0.0.0.0/1"), QStringLiteral("dev"), QStringLiteral("tun0")});
    QProcess::execute(QStringLiteral("ip"), {QStringLiteral("route"), QStringLiteral("del"),
        QStringLiteral("128.0.0.0/1"), QStringLiteral("dev"), QStringLiteral("tun0")});
#endif

    m_serverAddr.clear();
    m_originalGateway.clear();
    m_originalInterface.clear();
    m_tunInterfaceIndex = -1;

    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Routes cleaned up"));
}

void TunManager::onProcessStateChanged(QProcess::ProcessState state) {
    if (m_logBuffer) {
        QString stateStr;
        switch (state) {
            case QProcess::NotRunning: stateStr = QStringLiteral("NotRunning"); break;
            case QProcess::Starting: stateStr = QStringLiteral("Starting"); break;
            case QProcess::Running: stateStr = QStringLiteral("Running"); break;
        }
        m_logBuffer->append(QStringLiteral("[TUN] Process state: ") + stateStr);
    }
    if (state == QProcess::NotRunning) {
        cleanupRoutes();
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[TUN] Stopped"));
        emit stopped();
    }
    emit runningChanged();
}

void TunManager::onReadyReadStandardOutput() {
    if (!m_logBuffer) return;
    while (m_process->canReadLine())
        m_logBuffer->append(QStringLiteral("[TUN] ") + QString::fromUtf8(m_process->readLine()).trimmed());
}

void TunManager::onReadyReadStandardError() {
    if (!m_logBuffer) return;
    while (m_process->canReadLine())
        m_logBuffer->append(QStringLiteral("[TUN:err] ") + QString::fromUtf8(m_process->readLine()).trimmed());
}

void TunManager::onProcessError(QProcess::ProcessError error) {
    if (!m_logBuffer) return;
    QString errorStr;
    switch (error) {
        case QProcess::FailedToStart: errorStr = QStringLiteral("FailedToStart"); break;
        case QProcess::Crashed: errorStr = QStringLiteral("Crashed"); break;
        case QProcess::Timedout: errorStr = QStringLiteral("Timedout"); break;
        case QProcess::WriteError: errorStr = QStringLiteral("WriteError"); break;
        case QProcess::ReadError: errorStr = QStringLiteral("ReadError"); break;
        case QProcess::UnknownError: errorStr = QStringLiteral("UnknownError"); break;
    }
    m_logBuffer->append(QStringLiteral("[TUN] Process error: ") + errorStr + QStringLiteral(" - ") + m_process->errorString());
}

void TunManager::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (!m_logBuffer) return;
    QString statusStr = (exitStatus == QProcess::NormalExit) ? QStringLiteral("NormalExit") : QStringLiteral("CrashExit");
    m_logBuffer->append(QStringLiteral("[TUN] Process finished (exit code: %1, status: %2)").arg(exitCode).arg(statusStr));

    QByteArray remaining = m_process->readAllStandardError();
    if (!remaining.isEmpty()) {
        for (const QString &line : QString::fromUtf8(remaining).split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            QString trimmed = line.trimmed();
            if (!trimmed.isEmpty())
                m_logBuffer->append(QStringLiteral("[TUN:err] ") + trimmed);
        }
    }
}
