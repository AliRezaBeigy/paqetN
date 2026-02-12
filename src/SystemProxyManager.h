#pragma once

#include <QObject>
#include <QVariantMap>
#include <QStringList>

class LogBuffer;

class SystemProxyManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ isEnabled NOTIFY enabledChanged)
public:
    explicit SystemProxyManager(LogBuffer *logBuffer, QObject *parent = nullptr);
    ~SystemProxyManager() override;

    bool isEnabled() const { return m_enabled; }
    bool enable(int socksPort);
    void disable();

signals:
    void enabledChanged();

private:
#ifdef Q_OS_WIN
    bool enable_Windows(int socksPort);
    void disable_Windows();
    bool setConnectionProxy(const wchar_t *connectionName, const QString &proxyServer, const QString &exceptions, bool enable);
    QStringList getRasConnectionNames();
    void notifyProxyChange();
    bool enableViaRegistry(int socksPort);
    void disableViaRegistry();
#else
    bool enable_Linux(int socksPort);
    void disable_Linux();
#endif

    LogBuffer *m_logBuffer = nullptr;
    bool m_enabled = false;
    QVariantMap m_originalSettings;
};
