#pragma once

#include "PaqetConfig.h"
#include "LogBuffer.h"
#include <QObject>
#include <QProcess>

class PaqetRunner : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
public:
    explicit PaqetRunner(LogBuffer *logBuffer, QObject *parent = nullptr);

    bool isRunning() const { return m_process && m_process->state() != QProcess::NotRunning; }
    void start(const PaqetConfig &config, const QString &logLevel);

    void stop();
    void stopBlocking();

    QString resolvePaqetBinary() const;
    void setPaqetBinaryPath(const QString &path) { m_customPaqetPath = path; }

signals:
    void runningChanged();
    void started();
    void startFailed(const QString &error);
    void stopped();

private:
    void onProcessStateChanged(QProcess::ProcessState state);
    void onReadyReadStandardOutput();
    void onReadyReadStandardError();
    void onProcessError(QProcess::ProcessError error);
    void onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    LogBuffer *m_logBuffer = nullptr;
    QProcess *m_process = nullptr;
    QString m_customPaqetPath;
    QString m_configPath;
    qint64 m_registeredChildPid = 0;  // for CrashHandler unregister (Unix)
};
