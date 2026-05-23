#ifndef NOUS_ENGINE_LOGGERBINDINGS_H
#define NOUS_ENGINE_LOGGERBINDINGS_H

// --- Subsystem APIs ---
struct LoggerAPI
{
    // Basic variants — no source location (legacy, kept for backward compat).
    void (*Trace)(const char* msg, ...) = nullptr;
    void (*Debug)(const char* msg, ...) = nullptr;
    void (*Info) (const char* msg, ...) = nullptr;
    void (*Warn) (const char* msg, ...) = nullptr;
    void (*Error)(const char* msg, ...) = nullptr;
    void (*Fatal)(const char* msg, ...) = nullptr;

    // Source-location variants — called by NOUS_SCRIPT_* macros.
    // file/func are compile-time string literals from the script's call site.
    void (*TraceEx)(const char* file, int line, const char* func, const char* msg, ...) = nullptr;
    void (*DebugEx)(const char* file, int line, const char* func, const char* msg, ...) = nullptr;
    void (*InfoEx) (const char* file, int line, const char* func, const char* msg, ...) = nullptr;
    void (*WarnEx) (const char* file, int line, const char* func, const char* msg, ...) = nullptr;
    void (*ErrorEx)(const char* file, int line, const char* func, const char* msg, ...) = nullptr;
    void (*FatalEx)(const char* file, int line, const char* func, const char* msg, ...) = nullptr;
};

void SetupLoggerBindings(LoggerAPI& logger);

// ── Script-side macros ────────────────────────────────────────────────────────
// Use these in scripts instead of Nous_Engine->Logger->Debug() so that
// __FILE__, __LINE__, __FUNCTION__ resolve to the script's call site, not the binding layer.
//
// Usage:  NOUS_SCRIPT_DEBUG("speed = %.2f", speed);

namespace NousScriptLog {
    constexpr const char* FileName(const char* path) noexcept {
        const char* name = path;
        for (const char* p = path; *p; ++p)
            if (*p == '/' || *p == '\\') name = p + 1;
        return name;
    }
}
#define NOUS_SCRIPT_FILE NousScriptLog::FileName(__FILE__)

#define NOUS_SCRIPT_TRACE(msg, ...) Nous_Engine->Logger->TraceEx(NOUS_SCRIPT_FILE, __LINE__, __FUNCTION__, msg, ##__VA_ARGS__)
#define NOUS_SCRIPT_DEBUG(msg, ...) Nous_Engine->Logger->DebugEx(NOUS_SCRIPT_FILE, __LINE__, __FUNCTION__, msg, ##__VA_ARGS__)
#define NOUS_SCRIPT_INFO(msg, ...)  Nous_Engine->Logger->InfoEx (NOUS_SCRIPT_FILE, __LINE__, __FUNCTION__, msg, ##__VA_ARGS__)
#define NOUS_SCRIPT_WARN(msg, ...)  Nous_Engine->Logger->WarnEx (NOUS_SCRIPT_FILE, __LINE__, __FUNCTION__, msg, ##__VA_ARGS__)
#define NOUS_SCRIPT_ERROR(msg, ...) Nous_Engine->Logger->ErrorEx(NOUS_SCRIPT_FILE, __LINE__, __FUNCTION__, msg, ##__VA_ARGS__)
#define NOUS_SCRIPT_FATAL(msg, ...) Nous_Engine->Logger->FatalEx(NOUS_SCRIPT_FILE, __LINE__, __FUNCTION__, msg, ##__VA_ARGS__)

#endif //NOUS_ENGINE_LOGGERBINDINGS_H
