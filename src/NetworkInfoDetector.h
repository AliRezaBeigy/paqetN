#pragma once

#include <QObject>
#include <QString>
#include <QList>

class LogBuffer;

struct NetworkAdapterInfo {
    QString name;           // Friendly name (e.g., "Ethernet", "Wi-Fi")
    QString guid;           // Windows GUID in format \Device\NPF_{...}
    QString interfaceName;  // Unix interface name (e.g., "eth0", "en0")
    QString ipv4Address;    // Local IPv4 address
    QString gatewayIp;      // Gateway IP address
    QString gatewayMac;     // Gateway MAC address
    bool isActive;          // Is this adapter active/connected?
};

class NetworkInfoDetector : public QObject
{
    Q_OBJECT
public:
    explicit NetworkInfoDetector(QObject *parent = nullptr);
    void setLogBuffer(LogBuffer *logBuffer) { m_logBuffer = logBuffer; }
    void setLogLevel(const QString &logLevel) { m_logLevel = logLevel; }

    // Get list of all network adapters
    QList<NetworkAdapterInfo> detectAdapters();

    // Get the primary/default network adapter
    NetworkAdapterInfo getDefaultAdapter();

    // Get gateway MAC address for a given gateway IP
    QString getGatewayMac(const QString &gatewayIp);

private:
    void log(const QString &message);
    QString formatWindowsGuid(const QString &rawGuid);
    LogBuffer *m_logBuffer = nullptr;
    QString m_logLevel = QStringLiteral("fatal");  // Default to fatal to suppress logs by default
#ifdef Q_OS_WIN
    QList<NetworkAdapterInfo> detectAdaptersWindows();
    QString getGatewayMacWindows(const QString &gatewayIp);
#else
    QList<NetworkAdapterInfo> detectAdaptersUnix();
    QString getGatewayMacUnix(const QString &gatewayIp);
#endif
};
