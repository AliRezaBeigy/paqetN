#include "NetworkInfoDetector.h"
#include "LogBuffer.h"
#include <QProcess>
#include <QRegularExpression>
#include <QNetworkInterface>
#include <QHostAddress>
#include <QAbstractSocket>
#include <QDebug>

NetworkInfoDetector::NetworkInfoDetector(QObject *parent)
    : QObject(parent)
{
}

void NetworkInfoDetector::log(const QString &message)
{
    // Don't log network detection messages if log level is "fatal" or "none"
    if (m_logLevel == QStringLiteral("fatal") || m_logLevel == QStringLiteral("none")) {
        return;
    }
    
    QString logMsg = QStringLiteral("[NetworkDetect] ") + message;
    qDebug() << logMsg;
    if (m_logBuffer) {
        m_logBuffer->append(logMsg);
    }
}

QList<NetworkAdapterInfo> NetworkInfoDetector::detectAdapters()
{
#ifdef Q_OS_WIN
    return detectAdaptersWindows();
#else
    return detectAdaptersUnix();
#endif
}

// Helper function to check if IP is a real network IP (not APIPA/link-local)
static bool isRealNetworkIP(const QString &ipAddress)
{
    if (ipAddress.isEmpty()) return false;
    
    // Extract IP part (before :port)
    QString ip = ipAddress;
    int colonPos = ip.indexOf(QLatin1Char(':'));
    if (colonPos >= 0) {
        ip = ip.left(colonPos);
    }
    
    // Filter out APIPA/link-local addresses (169.254.x.x) - these indicate no DHCP/gateway
    if (ip.startsWith(QStringLiteral("169.254."))) {
        return false;
    }
    
    // Filter out loopback (127.x.x.x)
    if (ip.startsWith(QStringLiteral("127."))) {
        return false;
    }
    
    return true;
}

NetworkAdapterInfo NetworkInfoDetector::getDefaultAdapter()
{
    log(QStringLiteral("Getting default adapter..."));
    auto adapters = detectAdapters();

    // Filter out loopback adapters and APIPA addresses
    QList<NetworkAdapterInfo> candidates;
    
    for (const auto &adapter : adapters) {
        // Skip loopback interfaces
        if (adapter.ipv4Address.startsWith(QStringLiteral("127.")) ||
            adapter.interfaceName == QStringLiteral("lo") ||
            adapter.name.contains(QStringLiteral("Loopback"), Qt::CaseInsensitive)) {
            log(QStringLiteral("Skipping loopback adapter: '%1'").arg(adapter.name));
            continue;
        }
        
        // Must have IP address
        if (adapter.ipv4Address.isEmpty()) {
            continue;
        }
        
        // Skip APIPA/link-local addresses (169.254.x.x) - these indicate no DHCP/gateway
        if (!isRealNetworkIP(adapter.ipv4Address)) {
            log(QStringLiteral("Skipping adapter with APIPA/link-local IP: '%1', IP=%2")
                .arg(adapter.name, adapter.ipv4Address));
            continue;
        }
        
        candidates.append(adapter);
    }

    // Priority 1: Adapter with real IP + gateway + MAC (best - has full network config)
    // Gateway presence is more important than PowerShell's "active" status
    for (const auto &adapter : candidates) {
        if (!adapter.gatewayIp.isEmpty() && !adapter.gatewayMac.isEmpty()) {
            log(QStringLiteral("Selected default adapter (real IP+gateway+MAC): '%1', IP=%2, Gateway=%3, MAC=%4, GUID=%5, active=%6")
                .arg(adapter.name, adapter.ipv4Address, adapter.gatewayIp, adapter.gatewayMac, adapter.guid,
                     adapter.isActive ? QStringLiteral("true") : QStringLiteral("false")));
            return adapter;
        }
    }

    // Priority 2: Adapter with real IP + gateway (MAC might be empty, but has gateway)
    for (const auto &adapter : candidates) {
        if (!adapter.gatewayIp.isEmpty()) {
            log(QStringLiteral("Selected default adapter (real IP+gateway): '%1', IP=%2, Gateway=%3, GUID=%4, active=%5")
                .arg(adapter.name, adapter.ipv4Address, adapter.gatewayIp, adapter.guid,
                     adapter.isActive ? QStringLiteral("true") : QStringLiteral("false")));
            return adapter;
        }
    }

    // Priority 3: Active adapter with real IP (no gateway, but at least active)
    for (const auto &adapter : candidates) {
        if (adapter.isActive) {
            log(QStringLiteral("Selected default adapter (active with real IP): '%1', IP=%2, GUID=%3")
                .arg(adapter.name, adapter.ipv4Address, adapter.guid));
            return adapter;
        }
    }

    // Priority 4: Any adapter with real IP (fallback)
    if (!candidates.isEmpty()) {
        const auto &adapter = candidates.first();
        log(QStringLiteral("Selected default adapter (fallback with real IP): '%1', IP=%2, GUID=%3")
            .arg(adapter.name, adapter.ipv4Address, adapter.guid));
        return adapter;
    }

    log(QStringLiteral("WARNING: No suitable adapter found, returning empty adapter"));
    return NetworkAdapterInfo();
}

QString NetworkInfoDetector::getGatewayMac(const QString &gatewayIp)
{
#ifdef Q_OS_WIN
    return getGatewayMacWindows(gatewayIp);
#else
    return getGatewayMacUnix(gatewayIp);
#endif
}

QString NetworkInfoDetector::formatWindowsGuid(const QString &rawGuid)
{
    // Convert {XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX} to \\Device\\NPF_{...}
    QString cleaned = rawGuid.trimmed();
    if (cleaned.startsWith(QLatin1Char('{'))) {
        cleaned = cleaned.mid(1);
    }
    if (cleaned.endsWith(QLatin1Char('}'))) {
        cleaned.chop(1);
    }
    return QStringLiteral("\\\\Device\\\\NPF_{%1}").arg(cleaned);
}

#ifdef Q_OS_WIN

QList<NetworkAdapterInfo> NetworkInfoDetector::detectAdaptersWindows()
{
    log(QStringLiteral("Starting Windows network adapter detection..."));
    QList<NetworkAdapterInfo> result;

    // Step 1: Get adapter GUIDs and names using PowerShell
    log(QStringLiteral("Step 1: Running PowerShell Get-NetAdapter command..."));
    QProcess process;
    process.start(QStringLiteral("powershell.exe"),
                  QStringList() << QStringLiteral("-NoProfile")
                                << QStringLiteral("-Command")
                                << QStringLiteral("Get-NetAdapter | Select-Object Name, InterfaceGuid, Status | ConvertTo-Json"));

    if (!process.waitForFinished(5000)) {
        log(QStringLiteral("ERROR: PowerShell command timed out or failed"));
        return result;
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());
    log(QStringLiteral("PowerShell output length: %1 characters").arg(output.length()));

    // Parse JSON-like output
    // Format: [{"Name":"Ethernet","InterfaceGuid":"{...}","Status":"Up"}]
    QRegularExpression nameRe(QStringLiteral("\"Name\"\\s*:\\s*\"([^\"]+)\""));
    QRegularExpression guidRe(QStringLiteral("\"InterfaceGuid\"\\s*:\\s*\"([^\"]+)\""));
    QRegularExpression statusRe(QStringLiteral("\"Status\"\\s*:\\s*\"([^\"]+)\""));

    QStringList lines = output.split(QLatin1Char('\n'));
    NetworkAdapterInfo currentAdapter;

    for (const QString &line : lines) {
        QString trimmed = line.trimmed();

        auto nameMatch = nameRe.match(trimmed);
        if (nameMatch.hasMatch()) {
            currentAdapter.name = nameMatch.captured(1);
        }

        auto guidMatch = guidRe.match(trimmed);
        if (guidMatch.hasMatch()) {
            currentAdapter.guid = formatWindowsGuid(guidMatch.captured(1));
        }

        auto statusMatch = statusRe.match(trimmed);
        if (statusMatch.hasMatch()) {
            currentAdapter.isActive = (statusMatch.captured(1).toLower() == QLatin1String("up"));
        }

        // End of adapter entry
        if (trimmed.contains(QLatin1Char('}')) && !currentAdapter.name.isEmpty()) {
            // Skip loopback adapters
            if (currentAdapter.name.contains(QStringLiteral("Loopback"), Qt::CaseInsensitive) ||
                currentAdapter.name.contains(QStringLiteral("Loopback Pseudo"), Qt::CaseInsensitive)) {
                log(QStringLiteral("Skipping loopback adapter from PowerShell: '%1'").arg(currentAdapter.name));
            } else {
                log(QStringLiteral("Found adapter from PowerShell: name='%1', guid='%2', active=%3")
                    .arg(currentAdapter.name, currentAdapter.guid, currentAdapter.isActive ? QStringLiteral("true") : QStringLiteral("false")));
                result.append(currentAdapter);
            }
            currentAdapter = NetworkAdapterInfo();
        }
    }

    log(QStringLiteral("Total adapters found from PowerShell: %1").arg(result.size()));

    // Step 2: Get IP addresses and gateways using ipconfig /all
    log(QStringLiteral("Step 2: Running ipconfig /all command..."));
    process.start(QStringLiteral("ipconfig"), QStringList() << QStringLiteral("/all"));
    if (!process.waitForFinished(5000)) {
        log(QStringLiteral("ERROR: ipconfig command timed out or failed"));
        return result;
    }

    output = QString::fromUtf8(process.readAllStandardOutput());
    log(QStringLiteral("ipconfig output length: %1 characters").arg(output.length()));
    lines = output.split(QLatin1Char('\n'));

    QString currentAdapterName;
    QString ipAddress;
    QString gateway;
    QString description;

    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];
        QString trimmed = line.trimmed();

        // Detect adapter section - look for adapter name line
        if (line.contains(QLatin1String("adapter"), Qt::CaseInsensitive) && line.contains(QLatin1Char(':'))) {
            // Save previous adapter info
            if (!currentAdapterName.isEmpty() && !ipAddress.isEmpty()) {
                // Try to match by description or adapter name
                for (auto &adapter : result) {
                    bool matches = false;
                    QString matchReason;
                    
                    // Match by description (most reliable - usually matches exactly)
                    if (!description.isEmpty()) {
                        if (adapter.name.contains(description, Qt::CaseInsensitive) ||
                            description.contains(adapter.name, Qt::CaseInsensitive)) {
                            matches = true;
                            matchReason = QStringLiteral("description '%1'").arg(description);
                        }
                    }
                    
                    // Match by adapter name (ipconfig might say "Ethernet adapter Ethernet:" where PowerShell says "Ethernet")
                    if (!matches) {
                        // Normalize names: remove common prefixes/suffixes
                        QString normalizedAdapterName = adapter.name;
                        QString normalizedIpconfigName = currentAdapterName;
                        
                        // Try direct match
                        if (normalizedAdapterName.compare(normalizedIpconfigName, Qt::CaseInsensitive) == 0 ||
                            normalizedAdapterName.contains(normalizedIpconfigName, Qt::CaseInsensitive) ||
                            normalizedIpconfigName.contains(normalizedAdapterName, Qt::CaseInsensitive)) {
                            matches = true;
                            matchReason = QStringLiteral("name '%1' <-> '%2'").arg(adapter.name, currentAdapterName);
                        }
                    }
                    
                    if (matches) {
                        log(QStringLiteral("Matched adapter '%1' (%2) with ipconfig data: IP=%3, Gateway=%4")
                            .arg(adapter.name, matchReason, ipAddress, gateway));
                        adapter.ipv4Address = ipAddress + QStringLiteral(":0");
                        adapter.gatewayIp = gateway;
                        if (!gateway.isEmpty()) {
                            adapter.gatewayMac = getGatewayMacWindows(gateway);
                            log(QStringLiteral("Gateway MAC for %1: %2").arg(gateway, adapter.gatewayMac));
                        }
                        break;
                    }
                }
                
                if (!ipAddress.isEmpty() && currentAdapterName != QLatin1String("lo") && 
                    !currentAdapterName.contains(QStringLiteral("Loopback"), Qt::CaseInsensitive)) {
                    log(QStringLiteral("WARNING: Could not match ipconfig adapter '%1' (IP=%2, Gateway=%3) with any PowerShell adapter")
                        .arg(currentAdapterName, ipAddress, gateway));
                }
            }

            // Start new adapter
            QRegularExpression adapterNameRe(QStringLiteral("adapter\\s+(.+?)\\s*:"));
            auto match = adapterNameRe.match(line);
            if (match.hasMatch()) {
                currentAdapterName = match.captured(1).trimmed();
                log(QStringLiteral("Found ipconfig adapter section: '%1'").arg(currentAdapterName));
            }
            ipAddress.clear();
            gateway.clear();
            description.clear();
        }

        // Extract Description (helps with matching)
        if (trimmed.startsWith(QLatin1String("Description"), Qt::CaseInsensitive)) {
            QRegularExpression descRe(QStringLiteral(":\\s*(.+)"));
            auto match = descRe.match(trimmed);
            if (match.hasMatch()) {
                description = match.captured(1).trimmed();
                log(QStringLiteral("Found description for adapter '%1': '%2'").arg(currentAdapterName, description));
            }
        }

        // Extract IPv4 address
        if (trimmed.startsWith(QLatin1String("IPv4 Address"), Qt::CaseInsensitive)) {
            QRegularExpression ipRe(QStringLiteral("\\(Preferred\\)|:\\s*([0-9.]+)"));
            // Look for IP address pattern
            QRegularExpression ipPattern(QStringLiteral("([0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3})"));
            auto match = ipPattern.match(trimmed);
            if (match.hasMatch()) {
                ipAddress = match.captured(1);
                log(QStringLiteral("Found IPv4 address for adapter '%1': %2").arg(currentAdapterName, ipAddress));
            }
        }

        // Extract default gateway
        if (trimmed.startsWith(QLatin1String("Default Gateway"), Qt::CaseInsensitive)) {
            QRegularExpression gwPattern(QStringLiteral("([0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3}\\.[0-9]{1,3})"));
            auto match = gwPattern.match(trimmed);
            if (match.hasMatch()) {
                gateway = match.captured(1);
                log(QStringLiteral("Found gateway for adapter '%1': %2").arg(currentAdapterName, gateway));
            }
        }
    }

    // Save last adapter info
    if (!currentAdapterName.isEmpty() && !ipAddress.isEmpty()) {
        log(QStringLiteral("Processing last adapter: '%1', IP=%2, Gateway=%3, Description='%4'")
            .arg(currentAdapterName, ipAddress, gateway, description));
        bool matched = false;
        for (auto &adapter : result) {
            bool matches = false;
            QString matchReason;
            
            // Match by description first
            if (!description.isEmpty()) {
                if (adapter.name.contains(description, Qt::CaseInsensitive) ||
                    description.contains(adapter.name, Qt::CaseInsensitive)) {
                    matches = true;
                    matchReason = QStringLiteral("description '%1'").arg(description);
                }
            }
            
            // Match by name
            if (!matches) {
                if (adapter.name.compare(currentAdapterName, Qt::CaseInsensitive) == 0 ||
                    adapter.name.contains(currentAdapterName, Qt::CaseInsensitive) ||
                    currentAdapterName.contains(adapter.name, Qt::CaseInsensitive)) {
                    matches = true;
                    matchReason = QStringLiteral("name '%1' <-> '%2'").arg(adapter.name, currentAdapterName);
                }
            }
            
            if (matches) {
                log(QStringLiteral("  Matched adapter '%1' (%2)").arg(adapter.name, matchReason));
                log(QStringLiteral("  Setting IP and gateway for adapter '%1'").arg(adapter.name));
                adapter.ipv4Address = ipAddress + QStringLiteral(":0");
                adapter.gatewayIp = gateway;
                if (!gateway.isEmpty()) {
                    adapter.gatewayMac = getGatewayMacWindows(gateway);
                    log(QStringLiteral("  Gateway MAC: %1").arg(adapter.gatewayMac));
                }
                matched = true;
                break;
            }
        }
        if (!matched && currentAdapterName != QLatin1String("lo") && 
            !currentAdapterName.contains(QStringLiteral("Loopback"), Qt::CaseInsensitive)) {
            log(QStringLiteral("  WARNING: Could not match adapter '%1' with any PowerShell adapter").arg(currentAdapterName));
        }
    }

    // Step 3: Use QNetworkInterface as fallback for adapters without IP from ipconfig
    log(QStringLiteral("Step 3: Checking QNetworkInterface for missing data..."));
    const auto interfaces = QNetworkInterface::allInterfaces();
    for (const auto &iface : interfaces) {
        if (iface.flags() & QNetworkInterface::IsLoopBack) {
            continue;
        }

        // Check if we already have this adapter
        bool found = false;
        for (auto &adapter : result) {
            if (adapter.name == iface.humanReadableName()) {
                found = true;
                // Fill in missing IP if not already set
                if (adapter.ipv4Address.isEmpty()) {
                    const auto addresses = iface.addressEntries();
                    for (const auto &entry : addresses) {
                        if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                            adapter.ipv4Address = entry.ip().toString() + QStringLiteral(":0");
                            log(QStringLiteral("  Filled missing IP for '%1': %2").arg(adapter.name, adapter.ipv4Address));
                            break;
                        }
                    }
                }
                break;
            }
        }

        // Add new adapter if not found
        if (!found) {
            NetworkAdapterInfo adapter;
            adapter.name = iface.humanReadableName();
            adapter.isActive = (iface.flags() & QNetworkInterface::IsUp) &&
                              (iface.flags() & QNetworkInterface::IsRunning);
            
            const auto addresses = iface.addressEntries();
            for (const auto &entry : addresses) {
                if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                    adapter.ipv4Address = entry.ip().toString() + QStringLiteral(":0");
                    break;
                }
            }
            
            log(QStringLiteral("  Added adapter from QNetworkInterface: '%1', IP=%2, active=%3")
                .arg(adapter.name, adapter.ipv4Address, adapter.isActive ? QStringLiteral("true") : QStringLiteral("false")));
            result.append(adapter);
        }
    }

    log(QStringLiteral("Final adapter list (%1 adapters):").arg(result.size()));
    for (int i = 0; i < result.size(); ++i) {
        const auto &adapter = result[i];
        log(QStringLiteral("  [%1] name='%2', guid='%3', IP=%4, Gateway=%5, MAC=%6, active=%7")
            .arg(i + 1)
            .arg(adapter.name)
            .arg(adapter.guid)
            .arg(adapter.ipv4Address)
            .arg(adapter.gatewayIp)
            .arg(adapter.gatewayMac)
            .arg(adapter.isActive ? QStringLiteral("true") : QStringLiteral("false")));
    }

    return result;
}

QString NetworkInfoDetector::getGatewayMacWindows(const QString &gatewayIp)
{
    if (gatewayIp.isEmpty()) {
        return QString();
    }

    QProcess process;
    process.start(QStringLiteral("arp"), QStringList() << QStringLiteral("-a") << gatewayIp);

    if (!process.waitForFinished(3000)) {
        return QString();
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());

    // Parse ARP output: "  192.168.1.1           aa-bb-cc-dd-ee-ff     dynamic"
    QRegularExpression macRe(QStringLiteral("([0-9a-fA-F]{2}[:-]){5}[0-9a-fA-F]{2}"));
    auto match = macRe.match(output);

    if (match.hasMatch()) {
        QString mac = match.captured(0);
        // Convert Windows format (aa-bb-cc-dd-ee-ff) to standard (aa:bb:cc:dd:ee:ff)
        return mac.replace(QLatin1Char('-'), QLatin1Char(':'));
    }

    return QString();
}

#else

QList<NetworkAdapterInfo> NetworkInfoDetector::detectAdaptersUnix()
{
    QList<NetworkAdapterInfo> result;

    // Use QNetworkInterface for basic info
    const auto interfaces = QNetworkInterface::allInterfaces();

    for (const auto &iface : interfaces) {
        if (iface.flags() & QNetworkInterface::IsLoopBack) {
            continue;
        }

        NetworkAdapterInfo adapter;
        adapter.name = iface.humanReadableName();
        adapter.interfaceName = iface.name();
        adapter.isActive = (iface.flags() & QNetworkInterface::IsUp) &&
                          (iface.flags() & QNetworkInterface::IsRunning);

        // Get IPv4 address
        const auto addresses = iface.addressEntries();
        for (const auto &entry : addresses) {
            if (entry.ip().protocol() == QAbstractSocket::IPv4Protocol) {
                adapter.ipv4Address = entry.ip().toString() + QStringLiteral(":0");
                break;
            }
        }

        result.append(adapter);
    }

    // Get default gateway
    QProcess process;
#ifdef Q_OS_MACOS
    process.start(QStringLiteral("netstat"), QStringList() << QStringLiteral("-rn"));
#else
    process.start(QStringLiteral("ip"), QStringList() << QStringLiteral("route"));
#endif

    if (process.waitForFinished(3000)) {
        QString output = QString::fromUtf8(process.readAllStandardOutput());
        QRegularExpression gwRe(QStringLiteral("default\\s+(?:via\\s+)?([0-9.]+)"));
        auto match = gwRe.match(output);

        if (match.hasMatch()) {
            QString gateway = match.captured(1);

            // Assign gateway to first active adapter (simplified)
            for (auto &adapter : result) {
                if (adapter.isActive && !adapter.ipv4Address.isEmpty()) {
                    adapter.gatewayIp = gateway;
                    adapter.gatewayMac = getGatewayMacUnix(gateway);
                    break;
                }
            }
        }
    }

    return result;
}

QString NetworkInfoDetector::getGatewayMacUnix(const QString &gatewayIp)
{
    if (gatewayIp.isEmpty()) {
        return QString();
    }

    QProcess process;
    process.start(QStringLiteral("arp"), QStringList() << QStringLiteral("-n") << gatewayIp);

    if (!process.waitForFinished(3000)) {
        return QString();
    }

    QString output = QString::fromUtf8(process.readAllStandardOutput());

    // Parse ARP output
    QRegularExpression macRe(QStringLiteral("([0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}"));
    auto match = macRe.match(output);

    if (match.hasMatch()) {
        return match.captured(0);
    }

    return QString();
}

#endif
