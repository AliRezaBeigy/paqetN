#ifndef CRASHHANDLER_H
#define CRASHHANDLER_H

#include <QString>

/**
 * Installs a crash handler that on SIGSEGV/SIGABRT (Unix) or unhandled exception (Windows)
 * writes a diagnostic report to a file. Users can share this file to help debug.
 *
 * Call install() once early, e.g. from main() after QApplication is created so
 * applicationDirPath() is available. Report is written to reportDir/crash_report_<timestamp>.txt
 * (or crash_report.txt on Unix if timestamp is not safely available in handler).
 */
class CrashHandler
{
public:
    /** Install the crash handler. reportDir should be writable (e.g. applicationDirPath()). */
    static void install(const QString &reportDir);

    /** Optional: set the path to the executable for symbol resolution hints in the report. */
    static void setExecutablePath(const QString &path);

    /** Register a child process PID so it is killed when we crash (Unix only; on Windows job object is used). */
    static void registerChildPid(qint64 pid);
    /** Unregister a child process PID. */
    static void unregisterChildPid(qint64 pid);

private:
    CrashHandler() = delete;
};

#endif // CRASHHANDLER_H
