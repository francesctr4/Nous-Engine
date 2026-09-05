#include <CrashHandler/CrashHandler.h>

#include <cstdio>

#ifdef _WIN32

#include <Windows.h>
#include <DbgHelp.h>

namespace
{
    // Everything below runs inside an already-broken process. The rules that
    // follow from that, and they are the whole design:
    //
    //   - no engine allocator (NOUS_NEW routes into DynamicAllocator, whose
    //     mutex may be held by the thread that just died),
    //   - no Logger (its flush thread will never run again),
    //   - no C++ exceptions, no std::string, fixed stack buffers only.
    //
    // Anything richer than this is how a crash handler turns a diagnosable
    // crash into a hang.

    char s_appName[64] = "App";

    // Absolute "<exe dir>\Crashes", resolved ONCE at install time. Deliberately not a
    // relative "Crashes/": that is relative to the WORKING DIRECTORY, which differs by
    // launcher -- CLion runs from the project root, a terminal from bin/ -- so dumps
    // scattered into whichever directory happened to be current. Resolved at install
    // rather than in the handler, because a dying process is the wrong place to be
    // querying and concatenating paths.
    char s_dumpDir[MAX_PATH] = "Crashes";

    // A fault inside the handler must not re-enter it. Interlocked rather than a
    // plain bool because two threads can fault at once.
    LONG s_handling = 0;

    // Resolves an address to "module.dll+0x1234". This is the single most useful
    // line in the whole report: it is what turns a naked 0x7ffa972b151d into
    // "inside Nous-Engine.dll", which decides whether the bug is even ours.
    void DescribeAddress(void* address, char* out, size_t outSize)
    {
        HMODULE module = nullptr;

        if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                static_cast<LPCSTR>(address), &module) || !module)
        {
            snprintf(out, outSize, "<no module>");
            return;
        }

        char path[MAX_PATH] = {};
        if (GetModuleFileNameA(module, path, MAX_PATH) == 0)
        {
            snprintf(out, outSize, "<unnamed module>");
            return;
        }

        // Basename only -- the full path is noise in a one-line report.
        const char* name = path;
        for (const char* p = path; *p; ++p)
            if (*p == '\\' || *p == '/') name = p + 1;

        const auto base   = reinterpret_cast<uintptr_t>(module);
        const auto offset = reinterpret_cast<uintptr_t>(address) - base;

        snprintf(out, outSize, "%s+0x%llx", name, static_cast<unsigned long long>(offset));
    }

    const char* ExceptionName(const DWORD code)
    {
        switch (code)
        {
            case EXCEPTION_ACCESS_VIOLATION:      return "ACCESS_VIOLATION";
            case EXCEPTION_STACK_OVERFLOW:        return "STACK_OVERFLOW";
            case EXCEPTION_ILLEGAL_INSTRUCTION:   return "ILLEGAL_INSTRUCTION";
            case EXCEPTION_INT_DIVIDE_BY_ZERO:    return "INT_DIVIDE_BY_ZERO";
            case EXCEPTION_FLT_DIVIDE_BY_ZERO:    return "FLT_DIVIDE_BY_ZERO";
            case EXCEPTION_PRIV_INSTRUCTION:      return "PRIV_INSTRUCTION";
            case EXCEPTION_IN_PAGE_ERROR:         return "IN_PAGE_ERROR";
            case EXCEPTION_DATATYPE_MISALIGNMENT: return "DATATYPE_MISALIGNMENT";

            // Raised by __debugbreak(), which is what NOUS_ASSERT and every
            // VK_CHECK end in -- and also what clang emits for UB it proved
            // (`unreachable` becomes int3 under the MSVC ABI). If no assertion
            // text preceded this, suspect the latter.
            case EXCEPTION_BREAKPOINT:            return "BREAKPOINT";

            default:                              return "UNKNOWN";
        }
    }

    // Writes Crashes/<app>_<yyyymmdd-hhmmss>_<pid>.dmp. Returns false and leaves
    // outPath untouched on failure; the stderr report is printed either way, so a
    // failed dump still leaves the caller something.
    bool WriteDump(EXCEPTION_POINTERS* exceptionInfo, char* outPath, size_t outPathSize)
    {
        CreateDirectoryA(s_dumpDir, nullptr);   // fine if it already exists

        SYSTEMTIME t{};
        GetLocalTime(&t);

        snprintf(outPath, outPathSize, "%s\\%s_%04u%02u%02u-%02u%02u%02u_%lu.dmp",
                 s_dumpDir, s_appName, t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute,
                 t.wSecond, GetCurrentProcessId());

        const HANDLE file = CreateFileA(outPath, GENERIC_WRITE, 0, nullptr,
                                        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (file == INVALID_HANDLE_VALUE)
            return false;

        MINIDUMP_EXCEPTION_INFORMATION info{};
        info.ThreadId          = GetCurrentThreadId();
        info.ExceptionPointers = exceptionInfo;
        info.ClientPointers    = FALSE;

        // Deliberately NOT MiniDumpWithFullMemory: the engine holds a 50 MiB pool
        // plus host-visible GPU staging buffers, so a full dump is hundreds of MB
        // for information a stack trace almost never needs. This set gives every
        // thread's stack, the globals, and the loaded-module list -- enough for
        // `bt` and `image lookup`, which is what these dumps are for.
        constexpr auto type = static_cast<MINIDUMP_TYPE>(
            MiniDumpWithDataSegs |
            MiniDumpWithHandleData |
            MiniDumpWithThreadInfo |
            MiniDumpWithUnloadedModules);

        const BOOL ok = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
                                          file, type, &info, nullptr, nullptr);
        CloseHandle(file);
        return ok != FALSE;
    }

    LONG WINAPI CrashFilter(EXCEPTION_POINTERS* exceptionInfo)
    {
        if (InterlockedExchange(&s_handling, 1) != 0)
            return EXCEPTION_EXECUTE_HANDLER;   // already reporting a crash; just die

        const DWORD code    = exceptionInfo->ExceptionRecord->ExceptionCode;
        void* const address = exceptionInfo->ExceptionRecord->ExceptionAddress;

        char where[MAX_PATH + 32] = {};
        DescribeAddress(address, where, sizeof(where));

        // stderr FIRST and flushed, before anything that could itself fail. This
        // one line is what survives when the dump cannot be written at all.
        fflush(stdout);
        fprintf(stderr,
                "\n[CRASH] %s (0x%08lX) at 0x%p in %s | thread %lu\n",
                ExceptionName(code), static_cast<unsigned long>(code), address, where,
                GetCurrentThreadId());

        // ExceptionAddress is the faulting INSTRUCTION, not the address it touched.
        // For an access violation the latter is usually the more useful of the two
        // ("tried to write 0x0" names the bug outright), and it lives in
        // ExceptionInformation: [0] is 0 read / 1 write / 8 DEP, [1] the address.
        const auto& record = *exceptionInfo->ExceptionRecord;
        if ((code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR) &&
            record.NumberParameters >= 2)
        {
            const ULONG_PTR operation = record.ExceptionInformation[0];
            const auto      target    = reinterpret_cast<void*>(record.ExceptionInformation[1]);

            const char* verb = operation == 0 ? "reading"
                             : operation == 1 ? "writing"
                             : operation == 8 ? "executing (DEP)"
                                              : "accessing";

            char targetWhere[MAX_PATH + 32] = {};
            DescribeAddress(target, targetWhere, sizeof(targetWhere));

            fprintf(stderr, "[CRASH] while %s 0x%p (%s)\n", verb, target, targetWhere);
        }

        fflush(stderr);

        char dumpPath[MAX_PATH] = {};
        if (WriteDump(exceptionInfo, dumpPath, sizeof(dumpPath)))
            fprintf(stderr, "[CRASH] Minidump written to %s\n"
                            "[CRASH] Inspect with: lldb -c %s %s.exe\n",
                            dumpPath, dumpPath, s_appName);
        else
            fprintf(stderr, "[CRASH] Minidump could NOT be written (error %lu).\n",
                            GetLastError());

        fflush(stderr);

        return EXCEPTION_EXECUTE_HANDLER;   // terminate; no WER dialog
    }
}

namespace nous::engine::crash
{
    void InstallCrashHandler(const char* appName)
    {
        static bool installed = false;
        if (installed) return;
        installed = true;

        if (appName && *appName)
        {
            snprintf(s_appName, sizeof(s_appName), "%s", appName);
        }

        // Resolve <exe dir>\Crashes now, while the process is healthy. On failure the
        // "Crashes" default stands, so a dump still lands somewhere.
        char exePath[MAX_PATH] = {};
        if (GetModuleFileNameA(nullptr, exePath, MAX_PATH) != 0)
        {
            char* lastSep = nullptr;
            for (char* p = exePath; *p; ++p)
                if (*p == '\\' || *p == '/') lastSep = p;

            if (lastSep)
            {
                *lastSep = '\0';
                snprintf(s_dumpDir, sizeof(s_dumpDir), "%s\\Crashes", exePath);
            }
        }

        // Suppress the "program stopped working" dialog so an unattended or CI run
        // exits instead of blocking on a modal box.
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);

        SetUnhandledExceptionFilter(CrashFilter);
    }
}

#else // POSIX (Linux, macOS)

#include <csignal>
#include <cstring>
#include <cstdlib>
#include <execinfo.h>
#include <sys/resource.h>
#include <unistd.h>

namespace
{
    // The Windows rules apply here and one more, harder one: this runs in a SIGNAL
    // HANDLER, so only async-signal-safe functions are legal. printf/snprintf and
    // backtrace_symbols() all take the malloc lock -- which the faulting thread may
    // be holding -- and deadlock the process instead of reporting it. Hence write(2)
    // straight to STDERR_FILENO, hand-rolled formatting, and backtrace_symbols_fd,
    // which is the one variant documented not to allocate.

    char s_appName[64] = "App";

    volatile sig_atomic_t s_handling = 0;

    // A dedicated stack so a SIGSEGV caused by STACK OVERFLOW can still be reported:
    // on the normal stack there is by definition no room left to run the handler, and
    // the process would die silently. Fixed size rather than SIGSTKSZ, which stopped
    // being a compile-time constant in glibc 2.34.
    constexpr size_t k_AltStackSize = 64 * 1024;
    char s_altStack[k_AltStackSize];

    void WriteStr(const char* s)
    {
        if (!s) return;
        const ssize_t written = write(STDERR_FILENO, s, strlen(s));
        (void)written;   // nothing useful to do if even this fails
    }

    void WriteHex(unsigned long long value)
    {
        static const char digits[] = "0123456789abcdef";

        char buf[17];
        int  i = 16;
        buf[16] = '\0';

        if (value == 0)
        {
            WriteStr("0");
            return;
        }

        while (value != 0 && i > 0)
        {
            buf[--i] = digits[value & 0xF];
            value >>= 4;
        }

        const ssize_t written = write(STDERR_FILENO, buf + i, static_cast<size_t>(16 - i));
        (void)written;
    }

    const char* SignalName(const int sig)
    {
        switch (sig)
        {
            case SIGSEGV: return "SIGSEGV (invalid memory access)";
            case SIGBUS:  return "SIGBUS (bad address / misaligned access)";
            case SIGILL:  return "SIGILL (illegal instruction)";
            case SIGFPE:  return "SIGFPE (arithmetic error)";
            case SIGABRT: return "SIGABRT (abort)";

            // The POSIX counterpart of Windows' EXCEPTION_BREAKPOINT: NOUS_DebugBreak
            // is raise(SIGTRAP) here, so a failed NOUS_ASSERT or VK_CHECK lands on
            // this line. Clang's `unreachable` also becomes a trap instruction, so as
            // on Windows: SIGTRAP with no assertion text above it means UB the
            // optimizer proved, not an assertion.
            case SIGTRAP: return "SIGTRAP (breakpoint / assertion)";

            default:      return "signal";
        }
    }

    void CrashSignalHandler(int sig, siginfo_t* info, void*)
    {
        if (s_handling != 0)
            _exit(128 + sig);           // faulted while reporting; leave immediately

        s_handling = 1;

        WriteStr("\n[CRASH] ");
        WriteStr(s_appName);
        WriteStr(": ");
        WriteStr(SignalName(sig));
        WriteStr(" at 0x");
        WriteHex(reinterpret_cast<unsigned long long>(info ? info->si_addr : nullptr));
        WriteStr("\n");

        // Frames print as "module(+0xoffset)". Symbol names need the executable to
        // export them (-rdynamic); without that, resolve with
        //     addr2line -e <module> -fCi <offset>
        // which is enough, since the offset is what identifies the code.
        void*     frames[64];
        const int count = backtrace(frames, 64);
        backtrace_symbols_fd(frames, count, STDERR_FILENO);

        WriteStr("[CRASH] Re-raising for the OS to write a core file.\n"
                 "[CRASH] Find it with `coredumpctl gdb` (systemd) or in the cwd, then:\n"
                 "[CRASH]   lldb -c <core> ./");
        WriteStr(s_appName);
        WriteStr("\n");

        // Restore the default disposition and re-raise, so the kernel produces the
        // core it would have produced had we never installed a handler. Returning
        // instead would resume at the faulting instruction and loop forever.
        signal(sig, SIG_DFL);
        raise(sig);
    }
}

namespace nous::engine::crash
{
    void InstallCrashHandler(const char* appName)
    {
        static bool installed = false;
        if (installed) return;
        installed = true;

        if (appName && *appName)
        {
            snprintf(s_appName, sizeof(s_appName), "%s", appName);
        }

        // Raise the core-size soft limit to whatever the hard limit allows, so a
        // crash leaves a core without the user having remembered `ulimit -c unlimited`
        // first. A hard limit of 0 (some distros and CI images) still wins, and the
        // backtrace above is then all you get -- which is why the backtrace is printed
        // unconditionally rather than relying on the core.
        rlimit coreLimit{};
        if (getrlimit(RLIMIT_CORE, &coreLimit) == 0)
        {
            coreLimit.rlim_cur = coreLimit.rlim_max;
            setrlimit(RLIMIT_CORE, &coreLimit);
        }

        stack_t altStack{};
        altStack.ss_sp    = s_altStack;
        altStack.ss_size  = sizeof(s_altStack);
        altStack.ss_flags = 0;
        sigaltstack(&altStack, nullptr);

        struct sigaction action{};
        action.sa_sigaction = CrashSignalHandler;
        // SA_ONSTACK pairs with the alternate stack above; SA_RESETHAND makes the
        // disposition default again the moment we are entered, which is a second
        // guard against re-entering the handler from inside itself.
        action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_RESETHAND;
        sigemptyset(&action.sa_mask);

        for (const int sig : { SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT, SIGTRAP })
            sigaction(sig, &action, nullptr);
    }
}

#endif // _WIN32
