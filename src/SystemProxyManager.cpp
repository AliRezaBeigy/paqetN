#include "SystemProxyManager.h"
#include "LogBuffer.h"

#ifdef Q_OS_WIN
#include <windows.h>
#include <wininet.h>
#include <ras.h>
#include <raserror.h>
#pragma comment(lib, "rasapi32.lib")
#pragma comment(lib, "wininet.lib")
#else
#include <QProcess>
#include <QProcessEnvironment>
#endif

SystemProxyManager::SystemProxyManager(LogBuffer *logBuffer, QObject *parent)
    : QObject(parent), m_logBuffer(logBuffer) {}

SystemProxyManager::~SystemProxyManager() {
    if (m_enabled) disable();
}

bool SystemProxyManager::enable(int httpPort) {
    if (m_enabled) disable();

    if (m_logBuffer)
        m_logBuffer->append(QStringLiteral("[SystemProxy] Setting system proxy to HTTP 127.0.0.1:%1").arg(httpPort));

#ifdef Q_OS_WIN
    bool ok = enable_Windows(httpPort);
#else
    bool ok = enable_Linux(httpPort);
#endif

    if (ok) {
        m_enabled = true;
        emit enabledChanged();
        if (m_logBuffer)
            m_logBuffer->append(QStringLiteral("[SystemProxy] System proxy enabled"));
    } else {
        if (m_logBuffer)
            m_logBuffer->append(QStringLiteral("[SystemProxy] ERROR: Failed to set system proxy"));
    }
    return ok;
}

void SystemProxyManager::disable() {
    if (!m_enabled) return;

    if (m_logBuffer)
        m_logBuffer->append(QStringLiteral("[SystemProxy] Restoring original proxy settings..."));

#ifdef Q_OS_WIN
    disable_Windows();
#else
    disable_Linux();
#endif

    m_enabled = false;
    m_originalSettings.clear();
    emit enabledChanged();

    if (m_logBuffer)
        m_logBuffer->append(QStringLiteral("[SystemProxy] System proxy disabled"));
}

#ifdef Q_OS_WIN

// WinINet per-connection option structures (like v2rayN)
#ifndef INTERNET_PER_CONN_FLAGS
#define INTERNET_PER_CONN_FLAGS 1
#define INTERNET_PER_CONN_PROXY_SERVER 2
#define INTERNET_PER_CONN_PROXY_BYPASS 3
#define INTERNET_PER_CONN_FLAGS_UI 5
#define PROXY_TYPE_DIRECT 0x00000001
#define PROXY_TYPE_PROXY 0x00000002
#endif

struct INTERNET_PER_CONN_OPTIONW_CUSTOM {
    DWORD dwOption;
    union {
        DWORD dwValue;
        LPWSTR pszValue;
        FILETIME ftValue;
    } Value;
};

struct INTERNET_PER_CONN_OPTION_LISTW_CUSTOM {
    DWORD dwSize;
    LPWSTR pszConnection;
    DWORD dwOptionCount;
    DWORD dwOptionError;
    INTERNET_PER_CONN_OPTIONW_CUSTOM *pOptions;
};

bool SystemProxyManager::setConnectionProxy(const wchar_t *connectionName, const QString &proxyServer, const QString &exceptions, bool enable) {
    INTERNET_PER_CONN_OPTIONW_CUSTOM options[4];
    INTERNET_PER_CONN_OPTION_LISTW_CUSTOM list;

    std::wstring wProxyServer = proxyServer.toStdWString();
    std::wstring wExceptions = exceptions.toStdWString();

    // Option 1: Flags
    options[0].dwOption = INTERNET_PER_CONN_FLAGS;
    options[0].Value.dwValue = enable ? (PROXY_TYPE_DIRECT | PROXY_TYPE_PROXY) : PROXY_TYPE_DIRECT;

    // Option 2: Proxy server
    options[1].dwOption = INTERNET_PER_CONN_PROXY_SERVER;
    options[1].Value.pszValue = enable ? const_cast<wchar_t*>(wProxyServer.c_str()) : nullptr;

    // Option 3: Proxy bypass list
    options[2].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
    options[2].Value.pszValue = enable ? const_cast<wchar_t*>(wExceptions.c_str()) : nullptr;

    // Option 4: Flags UI (same as flags for consistency)
    options[3].dwOption = INTERNET_PER_CONN_FLAGS_UI;
    options[3].Value.dwValue = enable ? (PROXY_TYPE_DIRECT | PROXY_TYPE_PROXY) : PROXY_TYPE_DIRECT;

    list.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LISTW_CUSTOM);
    list.pszConnection = const_cast<wchar_t*>(connectionName);
    list.dwOptionCount = 4;
    list.dwOptionError = 0;
    list.pOptions = options;

    // Use InternetSetOption with INTERNET_OPTION_PER_CONNECTION_OPTION (75)
    bool success = InternetSetOptionW(nullptr, 75, &list, list.dwSize) != FALSE;

    return success;
}

QStringList SystemProxyManager::getRasConnectionNames() {
    QStringList connections;

    DWORD bufSize = 0;
    DWORD numEntries = 0;

    // First call to get required buffer size
    DWORD result = RasEnumEntriesW(nullptr, nullptr, nullptr, &bufSize, &numEntries);
    if (result != ERROR_BUFFER_TOO_SMALL && result != ERROR_SUCCESS) {
        return connections;
    }

    if (numEntries == 0 || bufSize == 0) {
        return connections;
    }

    QByteArray buffer(static_cast<int>(bufSize), 0);
    RASENTRYNAMEW *entries = reinterpret_cast<RASENTRYNAMEW*>(buffer.data());
    entries[0].dwSize = sizeof(RASENTRYNAMEW);

    result = RasEnumEntriesW(nullptr, nullptr, entries, &bufSize, &numEntries);
    if (result == ERROR_SUCCESS) {
        for (DWORD i = 0; i < numEntries; ++i) {
            connections.append(QString::fromWCharArray(entries[i].szEntryName));
        }
    }

    return connections;
}

void SystemProxyManager::notifyProxyChange() {
    // Notify the system that proxy settings changed (like v2rayN)
    InternetSetOptionW(nullptr, INTERNET_OPTION_SETTINGS_CHANGED, nullptr, 0);
    InternetSetOptionW(nullptr, INTERNET_OPTION_REFRESH, nullptr, 0);
}

bool SystemProxyManager::enable_Windows(int httpPort) {
    // Use HTTP proxy format (no protocol prefix = HTTP)
    QString proxyServer = QStringLiteral("127.0.0.1:%1").arg(httpPort);
    QString exceptions = QStringLiteral("<local>;localhost;127.*;10.*;172.16.*;192.168.*");

    if (m_logBuffer)
        m_logBuffer->append(QStringLiteral("[SystemProxy] Setting proxy via WinINet API: %1").arg(proxyServer));

    // Set proxy for LAN connection (null = LAN)
    bool success = setConnectionProxy(nullptr, proxyServer, exceptions, true);

    if (!success) {
        if (m_logBuffer)
            m_logBuffer->append(QStringLiteral("[SystemProxy] WinINet API failed, falling back to registry..."));

        // Fallback to registry method
        return enableViaRegistry(httpPort);
    }

    // Also set for RAS/dial-up connections
    QStringList rasConnections = getRasConnectionNames();
    for (const QString &conn : rasConnections) {
        std::wstring wConn = conn.toStdWString();
        setConnectionProxy(wConn.c_str(), proxyServer, exceptions, true);
    }

    // Store settings for later restore
    m_originalSettings.insert(QStringLiteral("proxyServer"), proxyServer);
    m_originalSettings.insert(QStringLiteral("exceptions"), exceptions);

    notifyProxyChange();

    if (m_logBuffer)
        m_logBuffer->append(QStringLiteral("[SystemProxy] System proxy set successfully via WinINet API"));

    return true;
}

bool SystemProxyManager::enableViaRegistry(int httpPort) {
    // Fallback registry method
    HKEY hKey = nullptr;
    LONG res = RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
        0, KEY_READ | KEY_WRITE, &hKey);

    if (res != ERROR_SUCCESS) {
        if (m_logBuffer)
            m_logBuffer->append(QStringLiteral("[SystemProxy] ERROR: Cannot open Internet Settings registry key"));
        return false;
    }

    // Save original values
    DWORD origEnable = 0;
    DWORD size = sizeof(DWORD);
    if (RegQueryValueExW(hKey, L"ProxyEnable", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&origEnable), &size) == ERROR_SUCCESS) {
        m_originalSettings.insert(QStringLiteral("ProxyEnable"), static_cast<int>(origEnable));
    }

    WCHAR origServer[512] = {};
    size = sizeof(origServer);
    if (RegQueryValueExW(hKey, L"ProxyServer", nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(origServer), &size) == ERROR_SUCCESS) {
        m_originalSettings.insert(QStringLiteral("ProxyServer"), QString::fromWCharArray(origServer));
    }

    // Set ProxyEnable = 1
    DWORD enable = 1;
    RegSetValueExW(hKey, L"ProxyEnable", 0, REG_DWORD,
                   reinterpret_cast<const BYTE *>(&enable), sizeof(DWORD));

    // Set ProxyServer (HTTP proxy - no protocol prefix)
    QString proxyServer = QStringLiteral("127.0.0.1:%1").arg(httpPort);
    std::wstring wProxyServer = proxyServer.toStdWString();
    RegSetValueExW(hKey, L"ProxyServer", 0, REG_SZ,
                   reinterpret_cast<const BYTE *>(wProxyServer.c_str()),
                   static_cast<DWORD>((wProxyServer.size() + 1) * sizeof(WCHAR)));

    // Set ProxyOverride
    const wchar_t *override = L"<local>;localhost;127.*;10.*;172.16.*;192.168.*";
    RegSetValueExW(hKey, L"ProxyOverride", 0, REG_SZ,
                   reinterpret_cast<const BYTE *>(override),
                   static_cast<DWORD>((wcslen(override) + 1) * sizeof(WCHAR)));

    RegCloseKey(hKey);

    m_originalSettings.insert(QStringLiteral("usedRegistry"), true);

    notifyProxyChange();

    return true;
}

void SystemProxyManager::disable_Windows() {
    // Check if we used registry fallback
    if (m_originalSettings.value(QStringLiteral("usedRegistry"), false).toBool()) {
        disableViaRegistry();
        return;
    }

    // Disable via WinINet API
    setConnectionProxy(nullptr, QString(), QString(), false);

    // Also disable for RAS connections
    QStringList rasConnections = getRasConnectionNames();
    for (const QString &conn : rasConnections) {
        std::wstring wConn = conn.toStdWString();
        setConnectionProxy(wConn.c_str(), QString(), QString(), false);
    }

    notifyProxyChange();

    if (m_logBuffer)
        m_logBuffer->append(QStringLiteral("[SystemProxy] System proxy disabled via WinINet API"));
}

void SystemProxyManager::disableViaRegistry() {
    HKEY hKey = nullptr;
    LONG res = RegOpenKeyExW(HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Internet Settings",
        0, KEY_WRITE, &hKey);

    if (res != ERROR_SUCCESS) return;

    // Restore original ProxyEnable
    DWORD origEnable = m_originalSettings.value(QStringLiteral("ProxyEnable"), 0).toUInt();
    RegSetValueExW(hKey, L"ProxyEnable", 0, REG_DWORD,
                   reinterpret_cast<const BYTE *>(&origEnable), sizeof(DWORD));

    // Restore original ProxyServer
    if (m_originalSettings.contains(QStringLiteral("ProxyServer"))) {
        std::wstring orig = m_originalSettings.value(QStringLiteral("ProxyServer")).toString().toStdWString();
        RegSetValueExW(hKey, L"ProxyServer", 0, REG_SZ,
                       reinterpret_cast<const BYTE *>(orig.c_str()),
                       static_cast<DWORD>((orig.size() + 1) * sizeof(WCHAR)));
    } else {
        RegDeleteValueW(hKey, L"ProxyServer");
    }

    RegCloseKey(hKey);

    notifyProxyChange();
}

#else // Linux

bool SystemProxyManager::enable_Linux(int httpPort) {
    // Detect desktop environment
    QString de = QProcessEnvironment::systemEnvironment().value(QStringLiteral("XDG_CURRENT_DESKTOP")).toLower();

    if (de.contains(QStringLiteral("gnome")) || de.contains(QStringLiteral("unity")) ||
        de.contains(QStringLiteral("cinnamon")) || de.contains(QStringLiteral("mate"))) {
        // GNOME-based: use gsettings
        // Save original mode
        QProcess readProc;
        readProc.start(QStringLiteral("gsettings"), {QStringLiteral("get"), QStringLiteral("org.gnome.system.proxy"), QStringLiteral("mode")});
        if (readProc.waitForFinished(3000)) {
            m_originalSettings.insert(QStringLiteral("gnome_mode"),
                QString::fromUtf8(readProc.readAllStandardOutput()).trimmed().remove(QLatin1Char('\'')));
        }

        QProcess::execute(QStringLiteral("gsettings"), {QStringLiteral("set"), QStringLiteral("org.gnome.system.proxy"), QStringLiteral("mode"), QStringLiteral("manual")});
        // Set HTTP proxy
        QProcess::execute(QStringLiteral("gsettings"), {QStringLiteral("set"), QStringLiteral("org.gnome.system.proxy.http"), QStringLiteral("host"), QStringLiteral("127.0.0.1")});
        QProcess::execute(QStringLiteral("gsettings"), {QStringLiteral("set"), QStringLiteral("org.gnome.system.proxy.http"), QStringLiteral("port"), QString::number(httpPort)});
        // Set HTTPS proxy
        QProcess::execute(QStringLiteral("gsettings"), {QStringLiteral("set"), QStringLiteral("org.gnome.system.proxy.https"), QStringLiteral("host"), QStringLiteral("127.0.0.1")});
        QProcess::execute(QStringLiteral("gsettings"), {QStringLiteral("set"), QStringLiteral("org.gnome.system.proxy.https"), QStringLiteral("port"), QString::number(httpPort)});

        m_originalSettings.insert(QStringLiteral("de"), QStringLiteral("gnome"));
        return true;
    }

    if (de.contains(QStringLiteral("kde")) || de.contains(QStringLiteral("plasma"))) {
        // KDE: use kwriteconfig
        QString kwriteconfig = QStringLiteral("kwriteconfig5");

        QProcess::execute(kwriteconfig, {QStringLiteral("--file"), QStringLiteral("kioslaverc"),
            QStringLiteral("--group"), QStringLiteral("Proxy Settings"),
            QStringLiteral("--key"), QStringLiteral("ProxyType"), QStringLiteral("1")});
        // Set HTTP proxy
        QProcess::execute(kwriteconfig, {QStringLiteral("--file"), QStringLiteral("kioslaverc"),
            QStringLiteral("--group"), QStringLiteral("Proxy Settings"),
            QStringLiteral("--key"), QStringLiteral("httpProxy"),
            QStringLiteral("http://127.0.0.1:%1").arg(httpPort)});
        // Set HTTPS proxy
        QProcess::execute(kwriteconfig, {QStringLiteral("--file"), QStringLiteral("kioslaverc"),
            QStringLiteral("--group"), QStringLiteral("Proxy Settings"),
            QStringLiteral("--key"), QStringLiteral("httpsProxy"),
            QStringLiteral("http://127.0.0.1:%1").arg(httpPort)});

        // Notify KDE
        QProcess::execute(QStringLiteral("dbus-send"), {
            QStringLiteral("--type=signal"), QStringLiteral("--dest=org.kde.kded5"),
            QStringLiteral("/kded"), QStringLiteral("org.kde.kded5.reloadConfiguration")});

        m_originalSettings.insert(QStringLiteral("de"), QStringLiteral("kde"));
        return true;
    }

    // Fallback: log a warning
    if (m_logBuffer)
        m_logBuffer->append(QStringLiteral("[SystemProxy] WARNING: Unsupported desktop environment '%1'. "
                                           "Set proxy manually: export http_proxy=http://127.0.0.1:%2").arg(de).arg(httpPort));
    return false;
}

void SystemProxyManager::disable_Linux() {
    QString de = m_originalSettings.value(QStringLiteral("de")).toString();

    if (de == QLatin1String("gnome")) {
        QString origMode = m_originalSettings.value(QStringLiteral("gnome_mode"), QStringLiteral("none")).toString();
        QProcess::execute(QStringLiteral("gsettings"), {QStringLiteral("set"), QStringLiteral("org.gnome.system.proxy"), QStringLiteral("mode"), origMode});
    } else if (de == QLatin1String("kde")) {
        QProcess::execute(QStringLiteral("kwriteconfig5"), {QStringLiteral("--file"), QStringLiteral("kioslaverc"),
            QStringLiteral("--group"), QStringLiteral("Proxy Settings"),
            QStringLiteral("--key"), QStringLiteral("ProxyType"), QStringLiteral("0")});
        QProcess::execute(QStringLiteral("dbus-send"), {
            QStringLiteral("--type=signal"), QStringLiteral("--dest=org.kde.kded5"),
            QStringLiteral("/kded"), QStringLiteral("org.kde.kded5.reloadConfiguration")});
    }
}

#endif
