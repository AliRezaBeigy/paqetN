#pragma once

#include <QObject>
#include <QProcess>
#include <QTimer>

class LogBuffer;

class TunManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
public:
    explicit TunManager(LogBuffer *logBuffer, QObject *parent = nullptr);

    bool isRunning() const { return m_process && m_process->state() != QProcess::NotRunning; }
    bool start(int socksPort, const QString &serverAddr);
    void stop();
    void stopBlocking();

    QString resolveTunBinary() const;
    void setTunBinaryPath(const QString &path) { m_customBinaryPath = path; }

signals:
    void runningChanged();
    void stopped();

private:
    void onProcessStateChanged(QProcess::ProcessState state);
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    void onServerRouteReady();
    void onProcessStartedForAsync();
    void onInterfacePoll();
    void onInterfaceTimeout();
    void onTunInterfaceReady();

    QString generateConfig(int socksPort);
    bool setupServerRoute(const QString &serverAddr);
    bool setupTunRoutes();
    void cleanupRoutes();
    void cancelAsyncStart();

#ifdef Q_OS_WIN
    int getTunInterfaceIndex() const;
    bool waitForTunInterface(int timeoutMs);
#endif

    LogBuffer *m_logBuffer = nullptr;
    QTimer *m_interfacePollTimer = nullptr;
    QTimer *m_interfaceTimeoutTimer = nullptr;
    bool m_asyncStartInProgress = false;
    QProcess *m_process = nullptr;
    QString m_customBinaryPath;
    QString m_configPath;
    QString m_serverAddr;
    QString m_originalGateway;
    QString m_originalInterface;
    int m_tunInterfaceIndex = -1;
    qint64 m_registeredChildPid = 0;  // for CrashHandler unregister (Unix)
};
