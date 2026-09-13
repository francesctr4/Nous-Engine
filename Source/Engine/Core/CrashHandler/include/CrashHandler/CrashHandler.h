#pragma once

#include <EngineCore/EngineExport.h>

// ============================================================================
// Process-wide crash handler.
//
// WINDOWS: on an unhandled exception this writes two things: a synchronous
// one-line report to stderr naming the exception code, the faulting address and
// the OWNING MODULE + OFFSET, and a minidump under Crashes/ next to the
// executable.
//
// LINUX / macOS: SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT and SIGTRAP print the
// same one-line report plus a backtrace, then re-raise so the kernel writes a
// real core file (the soft RLIMIT_CORE is raised at install time, so no
// `ulimit -c unlimited` is needed unless the HARD limit is 0). Find the core
// with `coredumpctl` under systemd, else in the working directory. No minidump
// is written there -- a core is strictly better and the platform makes it for
// free.
//
// The stderr line is the half that is always readable, and it is deliberately
// not routed through Logger: LogOutput only enqueues for the flush thread,
// which never gets scheduled in a dying process (see ReportAssertionFailure,
// which learned the same lesson). The dump is the half you can walk:
//
//     lldb -c Crashes/EditorApp_20260905-224156_23856.dmp EditorApp.exe
//     (lldb) bt
//     (lldb) image lookup -va <address>
//
// A dump is only as good as its symbols, which is why the Release-Windows
// preset builds with /Zi and links with /DEBUG.
//
// This also catches NOUS_ASSERT: __debugbreak() raises an unhandled
// EXCEPTION_BREAKPOINT when no debugger is attached, so a failed VK_CHECK
// produces a dump like any other crash.
//
// When a debugger IS attached it gets the exception first and this never runs,
// which is the desired behaviour -- you are already at the break.
// ============================================================================

namespace nous::engine::crash
{
    // Installs the handler. Call as the FIRST statement in main(), before the
    // memory system: it must outrank everything it may have to report on, and
    // it deliberately depends on nothing the engine sets up.
    //
    // `appName` names the dump file on Windows and labels the report on POSIX, so
    // pass something short and file-safe ("EditorApp", "GameApp"). Calling twice is
    // a no-op.
    NOUS_ENGINE_API void InstallCrashHandler(const char* appName);
}
