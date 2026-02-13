#include "PaqetRunner.h"
#include "ChildProcessJob.h"
#include "CrashHandler.h"
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QFileInfo>
#include <QTimer>

PaqetRunner::PaqetRunner(LogBuffer *logBuffer, QObject *parent)
    : QObject(parent), m_logBuffer(logBuffer) {
    m_process = new QProcess(this);
    connect(m_process, &QProcess::stateChanged, this, &PaqetRunner::onProcessStateChanged);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &PaqetRunner::onReadyReadStandardOutput);
    connect(m_process, &QProcess::readyReadStandardError, this, &PaqetRunner::onReadyReadStandardError);
    connect(m_process, &QProcess::errorOccurred, this, &PaqetRunner::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &PaqetRunner::onProcessFinished);
}

QString PaqetRunner::resolvePaqetBinary() const {
    if (!m_customPaqetPath.isEmpty()) {
        QFileInfo fi(m_customPaqetPath);
        if (fi.isExecutable()) return fi.absoluteFilePath();
    }
    const QString coresDir = QCoreApplication::applicationDirPath() + QLatin1String("/cores");
#ifdef Q_OS_WIN
    const QString exe = coresDir + QLatin1String("/paqet.exe");
#else
    const QString exe = coresDir + QLatin1String("/paqet");
#endif
    if (QFileInfo::exists(exe)) return exe;
    return QStringLiteral("paqet");
}

void PaqetRunner::start(const PaqetConfig &config, const QString &logLevel) {
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] Stopping any existing process..."));
    stop();

    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] Generating config YAML..."));
    const QString yaml = config.withDefaults().toYaml(logLevel);

    if (m_logBuffer) {
        m_logBuffer->append(QStringLiteral("[paqet] Generated YAML config:"));
        for (const QString &line : yaml.split(QLatin1Char('\n'))) {
            m_logBuffer->append(QStringLiteral("  ") + line);
        }
    }

    const QString dir = QStandardPaths::writableLocation(QStandardPaths::TempLocation) + QLatin1String("/paqetN");
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] Config directory: ") + dir);

    QDir().mkpath(dir);
    QFile f(dir + QLatin1String("/config_run.yaml"));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QString err = QStringLiteral("Failed to create config file: ") + f.fileName();
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] ERROR: ") + err);
        emit startFailed(err);
        return;
    }
    f.write(yaml.toUtf8());
    f.close();
    m_configPath = f.fileName();
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] Config written to: ") + m_configPath);

    const QString binary = resolvePaqetBinary();
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] Resolved binary: ") + binary);

    QFileInfo binaryInfo(binary);
    if (!binaryInfo.exists()) {
        QString err = QStringLiteral("Binary does not exist: ") + binary;
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] ERROR: ") + err);
        emit startFailed(err);
        return;
    }
    if (!binaryInfo.isExecutable()) {
        QString err = QStringLiteral("Binary is not executable: ") + binary;
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] ERROR: ") + err);
        emit startFailed(err);
        return;
    }

    m_process->setWorkingDirectory(dir);
    m_process->setProgram(binary);
    m_process->setArguments({ QStringLiteral("run"), QStringLiteral("-c"), m_configPath });
#ifndef Q_OS_WIN
    auto unixModifier = ChildProcessJob::childProcessModifier();
    if (unixModifier)
        m_process->setChildProcessModifier(unixModifier);
#endif

    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] Starting process: ") + binary + QLatin1String(" run -c ") + m_configPath);
    m_process->start(QProcess::ReadOnly);
    // started() or startFailed() will be emitted when process state is known
}

void PaqetRunner::stop() {
    if (!m_process || m_process->state() == QProcess::NotRunning) return;
    m_process->terminate();
    QTimer::singleShot(2000, this, [this]() {
        if (m_process && m_process->state() != QProcess::NotRunning) {
            m_process->kill();
        }
    });
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] Stopping..."));
}

void PaqetRunner::stopBlocking() {
    if (!m_process || m_process->state() == QProcess::NotRunning) return;
    m_process->terminate();
    if (!m_process->waitForFinished(2000))
        m_process->kill();
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] stopped"));
}

void PaqetRunner::onProcessStateChanged(QProcess::ProcessState state) {
    if (m_logBuffer) {
        QString stateStr;
        switch (state) {
            case QProcess::NotRunning: stateStr = QStringLiteral("NotRunning"); break;
            case QProcess::Starting: stateStr = QStringLiteral("Starting"); break;
            case QProcess::Running: stateStr = QStringLiteral("Running"); break;
        }
        m_logBuffer->append(QStringLiteral("[paqet] Process state changed to: ") + stateStr);
    }
    if (state == QProcess::Running) {
#ifdef Q_OS_WIN
        ChildProcessJob::assignProcess(m_process->processId());
#else
        m_registeredChildPid = m_process->processId();
        CrashHandler::registerChildPid(m_registeredChildPid);
#endif
        if (m_logBuffer)
            m_logBuffer->append(QStringLiteral("[paqet] Process started successfully (PID: %1)").arg(m_process->processId()));
        emit started();
    }
    if (state == QProcess::NotRunning) {
#ifndef Q_OS_WIN
        if (m_registeredChildPid != 0) {
            CrashHandler::unregisterChildPid(m_registeredChildPid);
            m_registeredChildPid = 0;
        }
#endif
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] stopped"));
        emit stopped();
    }
    emit runningChanged();
}

void PaqetRunner::onReadyReadStandardOutput() {
    if (!m_logBuffer) return;
    while (m_process->canReadLine())
        m_logBuffer->append(QString::fromUtf8(m_process->readLine()).trimmed());
}

void PaqetRunner::onReadyReadStandardError() {
    if (!m_logBuffer) return;
    while (m_process->canReadLine())
        m_logBuffer->append(QStringLiteral("[stderr] ") + QString::fromUtf8(m_process->readLine()).trimmed());
}

void PaqetRunner::onProcessError(QProcess::ProcessError error) {
    if (error == QProcess::FailedToStart) {
        QString err = m_process->errorString();
        if (m_logBuffer) {
            m_logBuffer->append(QStringLiteral("[paqet] ERROR: Failed to start process"));
            m_logBuffer->append(QStringLiteral("[paqet] Process error: ") + err);
        }
        emit startFailed(err);
        return;
    }
    if (!m_logBuffer) return;
    QString errorStr;
    switch (error) {
        case QProcess::FailedToStart:
            errorStr = QStringLiteral("FailedToStart - The process failed to start. Either the invoked program is missing, or you may have insufficient permissions.");
            break;
        case QProcess::Crashed:
            errorStr = QStringLiteral("Crashed - The process crashed some time after starting successfully.");
            break;
        case QProcess::Timedout:
            errorStr = QStringLiteral("Timedout - The process did not respond within the timeout period.");
            break;
        case QProcess::WriteError:
            errorStr = QStringLiteral("WriteError - An error occurred when attempting to write to the process.");
            break;
        case QProcess::ReadError:
            errorStr = QStringLiteral("ReadError - An error occurred when attempting to read from the process.");
            break;
        case QProcess::UnknownError:
            errorStr = QStringLiteral("UnknownError - An unknown error occurred.");
            break;
    }
    m_logBuffer->append(QStringLiteral("[paqet] Process error occurred: ") + errorStr);
    m_logBuffer->append(QStringLiteral("[paqet] Error details: ") + m_process->errorString());
}

void PaqetRunner::onProcessFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (!m_logBuffer) return;
    QString statusStr = (exitStatus == QProcess::NormalExit) ? QStringLiteral("NormalExit") : QStringLiteral("CrashExit");
    m_logBuffer->append(QStringLiteral("[paqet] Process finished with exit code: %1, status: %2").arg(exitCode).arg(statusStr));

    // Read any remaining output from stdout
    QByteArray stdoutData = m_process->readAllStandardOutput();
    if (!stdoutData.isEmpty()) {
        QString output = QString::fromUtf8(stdoutData);
        QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            m_logBuffer->append(QStringLiteral("[paqet] Remaining stdout:"));
            for (const QString &line : lines) {
                QString trimmed = line.trimmed();
                if (!trimmed.isEmpty()) {
                    m_logBuffer->append(trimmed);
                }
            }
        }
    }

    // Read any remaining output from stderr (most important for errors)
    QByteArray stderrData = m_process->readAllStandardError();
    if (!stderrData.isEmpty()) {
        QString output = QString::fromUtf8(stderrData);
        QStringList lines = output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (!lines.isEmpty()) {
            m_logBuffer->append(QStringLiteral("[paqet] Error output (stderr):"));
            for (const QString &line : lines) {
                QString trimmed = line.trimmed();
                if (!trimmed.isEmpty()) {
                    m_logBuffer->append(QStringLiteral("[stderr] ") + trimmed);
                }
            }
        }
    }

    // If process exited with error code, log it prominently
    if (exitCode != 0) {
        m_logBuffer->append(QStringLiteral("[paqet] ERROR: Process exited with code %1").arg(exitCode));
        if (stderrData.isEmpty() && stdoutData.isEmpty()) {
            m_logBuffer->append(QStringLiteral("[paqet] No error output captured. Check paqet binary and configuration."));
        }
    }
}
