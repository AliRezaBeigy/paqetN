#include "ChildProcessJob.h"

#ifdef Q_OS_WIN
#include <windows.h>

static HANDLE s_jobHandle = nullptr;

void ChildProcessJob::init()
{
    if (s_jobHandle != nullptr)
        return;
    s_jobHandle = CreateJobObjectW(nullptr, nullptr);
    if (s_jobHandle == nullptr)
        return;
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION info = {};
    info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    if (!SetInformationJobObject(s_jobHandle, JobObjectExtendedLimitInformation, &info, sizeof(info))) {
        CloseHandle(s_jobHandle);
        s_jobHandle = nullptr;
    }
}

bool ChildProcessJob::assignProcess(qint64 pid)
{
    if (s_jobHandle == nullptr || pid <= 0)
        return false;
    HANDLE hProcess = OpenProcess(PROCESS_SET_QUOTA | PROCESS_TERMINATE, FALSE, static_cast<DWORD>(pid));
    if (hProcess == nullptr)
        return false;
    const BOOL ok = AssignProcessToJobObject(s_jobHandle, hProcess);
    CloseHandle(hProcess);
    return ok != 0;
}

std::function<void()> ChildProcessJob::childProcessModifier()
{
    return std::function<void()>();
}

#else
#if defined(__linux__)
#include <signal.h>
#include <sys/prctl.h>
#ifndef PR_SET_PDEATHSIG
#define PR_SET_PDEATHSIG 1
#endif
#endif

void ChildProcessJob::init()
{
}

bool ChildProcessJob::assignProcess(qint64)
{
    return true;
}

std::function<void()> ChildProcessJob::childProcessModifier()
{
#if defined(__linux__)
    return []() {
#ifdef PR_SET_PDEATHSIG
        prctl(PR_SET_PDEATHSIG, SIGKILL);
#endif
    };
#else
    return std::function<void()>();
#endif
}
#endif
