#include "PaqetRunner.h"
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QTemporaryFile>
#include <QFileInfo>

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
    const QString baseDir = QCoreApplication::applicationDirPath();
#ifdef Q_OS_WIN
    const QString exe = baseDir + QLatin1String("/paqet.exe");
#else
    const QString exe = baseDir + QLatin1String("/paqet");
#endif
    if (QFileInfo::exists(exe)) return exe;
    return QStringLiteral("paqet");
}

bool PaqetRunner::start(const PaqetConfig &config, const QString &logLevel) {
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] Stopping any existing process..."));
    stop();

    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] Generating config YAML..."));
    const QString yaml = config.withDefaults().toYaml(logLevel);

    // Log the generated YAML for debugging
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
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] ERROR: Failed to create config file: ") + f.fileName());
        return false;
    }
    f.write(yaml.toUtf8());
    f.close();
    m_configPath = f.fileName();
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] Config written to: ") + m_configPath);

    const QString binary = resolvePaqetBinary();
    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] Resolved binary: ") + binary);

    QFileInfo binaryInfo(binary);
    if (!binaryInfo.exists()) {
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] ERROR: Binary does not exist: ") + binary);
        return false;
    }
    if (!binaryInfo.isExecutable()) {
        if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] ERROR: Binary is not executable: ") + binary);
        return false;
    }

    m_process->setWorkingDirectory(dir);
    m_process->setProgram(binary);
    m_process->setArguments({ QStringLiteral("run"), QStringLiteral("-c"), m_configPath });

    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] Starting process: ") + binary + QLatin1String(" run -c ") + m_configPath);
    m_process->start(QProcess::ReadOnly);

    if (m_logBuffer) m_logBuffer->append(QStringLiteral("[paqet] Waiting for process to start (3 seconds timeout)..."));
    const bool ok = m_process->waitForStarted(3000);

    if (ok) {
        if (m_logBuffer) {
            m_logBuffer->append(QStringLiteral("[paqet] Process started successfully (PID: %1)").arg(m_process->processId()));
            m_logBuffer->append(QStringLiteral("[paqet] Process state: %1").arg(m_process->state()));
        }
    } else {
        if (m_logBuffer) {
            m_logBuffer->append(QStringLiteral("[paqet] ERROR: Failed to start process"));
            m_logBuffer->append(QStringLiteral("[paqet] Process error: %1").arg(m_process->errorString()));
            m_logBuffer->append(QStringLiteral("[paqet] Process state: %1").arg(m_process->state()));
            m_logBuffer->append(QStringLiteral("[paqet] Exit code: %1").arg(m_process->exitCode()));
            m_logBuffer->append(QStringLiteral("[paqet] Exit status: %1").arg(m_process->exitStatus()));
        }
    }

    return ok;
}

void PaqetRunner::stop() {
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
