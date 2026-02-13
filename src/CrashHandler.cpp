#include "CrashHandler.h"
#include <QStandardPaths>
#include <QDateTime>
#include <QDir>
#include <cstdio>
#include <cstring>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
#include <psapi.h>
#pragma comment(lib, "dbghelp.lib")
#pragma comment(lib, "psapi.lib")
#endif

#if defined(__unix__) || defined(__APPLE__)
#include <signal.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#if defined(__linux__) || defined(__APPLE__)
#include <execinfo.h>
#endif
#endif

static QString s_reportDir;
static QString s_executablePath;
#ifdef Q_OS_WIN
static constexpr size_t kMaxFrames = 64;
static wchar_t s_reportDirW[MAX_PATH * 2] = {};
#endif
#if defined(__unix__) || defined(__APPLE__)
static int s_crashReportFd = -1;
static char s_crashReportPath[1024] = {};
#include <atomic>
#include <csignal>
static constexpr int kMaxChildPids = 64;
static pid_t s_childPids[kMaxChildPids];
static std::atomic<int> s_childPidCount{0};
#endif

#ifdef Q_OS_WIN
static LONG WINAPI unhandledExceptionFilter(EXCEPTION_POINTERS *pExceptionInfo)
{
    const DWORD code = pExceptionInfo->ExceptionRecord->ExceptionCode;
    const ULONG_PTR addr = (ULONG_PTR)pExceptionInfo->ExceptionRecord->ExceptionAddress;

    wchar_t reportPathW[MAX_PATH * 2] = {};
    if (s_reportDirW[0] == L'\0') {
        return EXCEPTION_EXECUTE_HANDLER;
    }
    SYSTEMTIME st;
    GetLocalTime(&st);
    swprintf(reportPathW, sizeof(reportPathW) / sizeof(wchar_t),
             L"%s\\crash_report_%04u-%02u-%02u_%02u-%02u-%02u.txt",
             s_reportDirW,
             (unsigned)st.wYear, (unsigned)st.wMonth, (unsigned)st.wDay,
             (unsigned)st.wHour, (unsigned)st.wMinute, (unsigned)st.wSecond);

    HANDLE hFile = CreateFileW(
        reportPathW,
        GENERIC_WRITE,
        FILE_SHARE_READ,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL,
        nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        return EXCEPTION_EXECUTE_HANDLER;
    }

    char line[1024];
    int len;
    DWORD written = 0;

    len = snprintf(line, sizeof(line),
        "paqetN crash report\n"
        "===================\n"
        "Please share this file with the developers to help fix the issue.\n\n"
        "Time: %04u-%02u-%02u %02u:%02u:%02u\n"
        "Exception: 0x%08lX",
        (unsigned)st.wYear, (unsigned)st.wMonth, (unsigned)st.wDay,
        (unsigned)st.wHour, (unsigned)st.wMinute, (unsigned)st.wSecond,
        (unsigned long)code);
    if (len > 0) {
        WriteFile(hFile, line, (DWORD)len, &written, nullptr);
    }

    len = snprintf(line, sizeof(line), " at address %p\n\n", (void *)addr);
    if (len > 0) {
        WriteFile(hFile, line, (DWORD)len, &written, nullptr);
    }

    // Initialize symbol handler for this process
    HANDLE hProcess = GetCurrentProcess();
    if (SymInitialize(hProcess, nullptr, TRUE)) {
        SymSetOptions(SYMOPT_LOAD_LINES | SYMOPT_UNDNAME);

        STACKFRAME64 frame = {};
        frame.AddrPC.Mode = AddrModeFlat;
        frame.AddrFrame.Mode = AddrModeFlat;
        frame.AddrStack.Mode = AddrModeFlat;
#ifdef _M_X64
        frame.AddrPC.Offset = pExceptionInfo->ContextRecord->Rip;
        frame.AddrFrame.Offset = pExceptionInfo->ContextRecord->Rbp;
        frame.AddrStack.Offset = pExceptionInfo->ContextRecord->Rsp;
        const DWORD machine = IMAGE_FILE_MACHINE_AMD64;
#else
        frame.AddrPC.Offset = (DWORD64)pExceptionInfo->ContextRecord->Eip;
        frame.AddrFrame.Offset = (DWORD64)pExceptionInfo->ContextRecord->Ebp;
        frame.AddrStack.Offset = (DWORD64)pExceptionInfo->ContextRecord->Esp;
        const DWORD machine = IMAGE_FILE_MACHINE_I386;
#endif
        const char *stackHeader = "Stack trace:\n";
        WriteFile(hFile, stackHeader, (DWORD)strlen(stackHeader), &written, nullptr);

        DWORD64 symBuf[sizeof(SYMBOL_INFO) + 256];
        PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)symBuf;
        pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        pSymbol->MaxNameLen = 255;

        IMAGEHLP_LINE64 lineInfo = {};
        lineInfo.SizeOfStruct = sizeof(IMAGEHLP_LINE64);

        for (size_t i = 0; i < kMaxFrames; i++) {
            if (!StackWalk64(machine, hProcess, GetCurrentThread(), &frame,
                             pExceptionInfo->ContextRecord, nullptr,
                             SymFunctionTableAccess64, SymGetModuleBase64, nullptr)) {
                break;
            }
            if (frame.AddrPC.Offset == 0) {
                break;
            }

            DWORD64 addr = frame.AddrPC.Offset;
            const char *symbolName = "???";
            const char *fileName = nullptr;
            DWORD lineNum = 0;

            if (SymFromAddr(hProcess, addr, nullptr, pSymbol)) {
                symbolName = pSymbol->Name;
            }
            if (SymGetLineFromAddr64(hProcess, addr, (PDWORD)&lineNum, &lineInfo)) {
                fileName = lineInfo.FileName;
            }

            if (fileName && lineNum) {
                len = snprintf(line, sizeof(line), "  #%zu  %p  %s  %s:%lu\n",
                              i, (void *)(uintptr_t)addr, symbolName, fileName, (unsigned long)lineNum);
            } else {
                len = snprintf(line, sizeof(line), "  #%zu  %p  %s\n",
                              i, (void *)(uintptr_t)addr, symbolName);
            }
            if (len > 0) {
                WriteFile(hFile, line, (DWORD)len, &written, nullptr);
            }
        }
        SymCleanup(hProcess);
    } else {
        const char *fallback = "StackWalk64/SymInitialize failed. Report path for manual inspection:\n";
        WriteFile(hFile, fallback, (DWORD)strlen(fallback), &written, nullptr);
    }

    char reportPathUtf8[MAX_PATH * 3] = {};
    const int pathUtf8Len = WideCharToMultiByte(CP_UTF8, 0, reportPathW, -1, reportPathUtf8, (int)sizeof(reportPathUtf8), nullptr, nullptr);
    if (pathUtf8Len > 0) {
        len = snprintf(line, sizeof(line), "\nReport file: %s\n", reportPathUtf8);
        if (len > 0) {
            WriteFile(hFile, line, (DWORD)len, &written, nullptr);
        }
    }

    CloseHandle(hFile);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

#if defined(__unix__) || defined(__APPLE__)
static void writeStr(int fd, const char *s)
{
    if (fd < 0 || !s) return;
    size_t len = strlen(s);
    while (len > 0) {
        ssize_t n = write(fd, s, len);
        if (n <= 0) break;
        s += n;
        len -= (size_t)n;
    }
}

static void crashSignalHandler(int sig)
{
    // Kill child processes (paqet, hev-socks5-tunnel) so they don't outlive us
    const int n = s_childPidCount.load(std::memory_order_acquire);
    for (int i = 0; i < n && i < kMaxChildPids; i++) {
        pid_t p = s_childPids[i];
        if (p > 0)
            kill(p, SIGKILL);
    }
    if (s_crashReportFd >= 0) {
        char header[512];
        snprintf(header, sizeof(header),
            "paqetN crash report\n"
            "===================\n"
            "Please share this file with the developers to help fix the issue.\n\n"
            "Signal: %d (%s)\n\n",
            sig,
            sig == SIGSEGV ? "SIGSEGV" : sig == SIGABRT ? "SIGABRT" : sig == SIGFPE ? "SIGFPE" : "?");
        writeStr(s_crashReportFd, header);

#if defined(__linux__) || defined(__APPLE__)
        void *bt[64];
        int n = backtrace(bt, 64);
        backtrace_symbols_fd(bt, n, s_crashReportFd);
#else
        writeStr(s_crashReportFd, "Stack trace (raw); rebuild with -rdynamic on Linux for symbols.\n");
#endif
        writeStr(s_crashReportFd, "\nReport file: ");
        writeStr(s_crashReportFd, s_crashReportPath);
        writeStr(s_crashReportFd, "\n");
    }
    _exit(1);
}
#endif

void CrashHandler::install(const QString &reportDir)
{
    s_reportDir = reportDir;
    if (!s_reportDir.isEmpty()) {
        QDir().mkpath(s_reportDir);
    }

#ifdef Q_OS_WIN
    if (!s_reportDir.isEmpty()) {
        const int pathLen = s_reportDir.toWCharArray(s_reportDirW);
        if (pathLen > 0 && pathLen < (int)(sizeof(s_reportDirW) / sizeof(wchar_t))) {
            s_reportDirW[pathLen] = L'\0';
        }
    }
    if (s_reportDirW[0] == L'\0') {
        wchar_t tmp[MAX_PATH];
        if (GetTempPathW(MAX_PATH, tmp)) {
            const size_t cap = sizeof(s_reportDirW) / sizeof(wchar_t);
            size_t len = wcslen(tmp);
            if (len >= cap) len = cap - 1;
            wmemcpy(s_reportDirW, tmp, len);
            s_reportDirW[len] = L'\0';
        }
    }
    SetUnhandledExceptionFilter(unhandledExceptionFilter);
#endif

#if defined(__unix__) || defined(__APPLE__)
    const QString path = reportDir.isEmpty()
        ? QString::fromUtf8(getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp")
        : reportDir;
    const QString fullPath = path + QDir::separator() + QStringLiteral("paqetN_crash_report.txt");
    QByteArray pathBa = fullPath.toUtf8();
    if (pathBa.size() < (int)sizeof(s_crashReportPath)) {
        strncpy(s_crashReportPath, pathBa.constData(), sizeof(s_crashReportPath) - 1);
        s_crashReportPath[sizeof(s_crashReportPath) - 1] = '\0';
    }
    s_crashReportFd = open(s_crashReportPath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (s_crashReportFd < 0) {
        s_crashReportFd = 2; // fallback to stderr
        s_crashReportPath[0] = '\0';
    }

    struct sigaction sa = {};
    sa.sa_handler = crashSignalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER | SA_RESETHAND;
    sigaction(SIGSEGV, &sa, nullptr);
    sigaction(SIGABRT, &sa, nullptr);
    sigaction(SIGFPE, &sa, nullptr);
#endif
}

void CrashHandler::setExecutablePath(const QString &path)
{
    s_executablePath = path;
}

void CrashHandler::registerChildPid(qint64 pid)
{
#if defined(__unix__) || defined(__APPLE__)
    pid_t p = static_cast<pid_t>(pid);
    if (p <= 0) return;
    int c = s_childPidCount.load(std::memory_order_relaxed);
    if (c < kMaxChildPids) {
        s_childPids[c] = p;
        s_childPidCount.store(c + 1, std::memory_order_release);
    }
#else
    (void)pid;
#endif
}

void CrashHandler::unregisterChildPid(qint64 pid)
{
#if defined(__unix__) || defined(__APPLE__)
    pid_t p = static_cast<pid_t>(pid);
    if (p <= 0) return;
    int n = s_childPidCount.load(std::memory_order_acquire);
    for (int i = 0; i < n; i++) {
        if (s_childPids[i] == p) {
            s_childPids[i] = s_childPids[n - 1];
            s_childPidCount.store(n - 1, std::memory_order_release);
            break;
        }
    }
#else
    (void)pid;
#endif
}
