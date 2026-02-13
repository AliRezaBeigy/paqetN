#pragma once

#include <QtGlobal>
#include <functional>

/**
 * Ensures child processes (paqet, hev-socks5-tunnel) are killed when PaqetN exits,
 * including on crash/segfault.
 *
 * - Windows: Uses a Job Object with JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE. When
 *   PaqetN exits, the job handle is closed and all processes in the job are terminated.
 * - Unix: Use setChildProcessModifier() with prctl(PR_SET_PDEATHSIG, SIGKILL) so
 *   the kernel kills the child when the parent dies (see ChildProcessJob::childProcessModifier).
 */
class ChildProcessJob
{
public:
    /** Call once at startup (Windows only). No-op on Unix. */
    static void init();

    /**
     * Assign a running child process to the job (Windows only).
     * Call after QProcess::start() and waitForStarted(); pass m_process->processId().
     * Returns true on success.
     */
    static bool assignProcess(qint64 pid);

    /**
     * Returns a modifier suitable for QProcess::setChildProcessModifier() on Unix.
     * Sets PR_SET_PDEATHSIG so the child receives SIGKILL when the parent exits.
     * Returns an empty std::function on Windows (use assignProcess after start instead).
     */
    static std::function<void()> childProcessModifier();

private:
    ChildProcessJob() = delete;
};
