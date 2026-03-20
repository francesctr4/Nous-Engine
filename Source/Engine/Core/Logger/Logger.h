#ifndef LOGGER_H
#define LOGGER_H

#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"
#include "LogChannel.h"

#include <vector>
#include <string>
#include <functional>
#include <deque>
#include <cstdint>

#define LOG_WARN_ENABLED 1
#define LOG_INFO_ENABLED 1

#ifdef _DEBUG
#define LOG_DEBUG_ENABLED 1
#define LOG_TRACE_ENABLED 1
#else
#define LOG_DEBUG_ENABLED 0
#define LOG_TRACE_ENABLED 0
#endif // _DEBUG

typedef enum LogLevel {

	NONE = -1,

	LOG_LEVEL_FATAL = 0,
	LOG_LEVEL_ERROR = 1,
	LOG_LEVEL_WARN  = 2,
	LOG_LEVEL_INFO  = 3,

	// Exclusively on Debug builds
	LOG_LEVEL_DEBUG = 4,
	LOG_LEVEL_TRACE = 5,

	LOG_LEVEL_MAX

} LogLevel;

// ──────────────────────────────────────────────────────────────────────────────
// LogEntry — canonical record type used by history and ConsoleWindow
// ──────────────────────────────────────────────────────────────────────────────

struct NOUS_ENGINE_API LogEntry
{
	LogLevel    level     = LOG_LEVEL_INFO;
	LogChannel  channel   = LogChannel::DEFAULT;
	double      timestamp = 0.0;   // seconds since engine start
	std::string message;           // fully formatted: "[LEVEL]: text\n"

	// Source location — populated when called through NOUS_* macros.
	// Points to compile-time string literals; lifetime is static (no allocation).
	const char* file     = nullptr; // __FILE__
	int         line     = 0;       // __LINE__
	const char* function = nullptr; // __FUNCTION__

	// Thread ID captured at log time (Win32 GetCurrentThreadId or hash of std::thread::id).
	uint64_t    threadId = 0;
};

// ──────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ──────────────────────────────────────────────────────────────────────────────

// Call as the very first line of main() to anchor all log timestamps to program start.
// InitializeLogging() calls this automatically as a fallback if you forget.
NOUS_ENGINE_API void StartLogTimer();

NOUS_ENGINE_API bool InitializeLogging();
NOUS_ENGINE_API void ShutdownLogging();

// ──────────────────────────────────────────────────────────────────────────────
// Log output
// ──────────────────────────────────────────────────────────────────────────────

// Direct file write, bypasses async queue — rarely needed externally.
NOUS_ENGINE_API void AppendToLogFile(const char* message);

NOUS_ENGINE_API void LogOutput(LogLevel level, const char* message, ...);
NOUS_ENGINE_API void LogOutput(LogLevel level, LogChannel channel, const char* message, ...);

// Extended variant — called by NOUS_* macros to capture source location + thread ID.
// Do not call directly; use the macros so __FILE__/__LINE__/__FUNCTION__ resolve correctly.
NOUS_ENGINE_API void LogOutputEx(LogLevel level, LogChannel channel,
                                 const char* file, int line, const char* function,
                                 const char* message, ...);

// Splits `text` on '\n' and emits each non-empty line as a separate log entry.
// Use this for multi-line strings (e.g. stat dumps) so ConsoleWindow's
// per-line clipper renders them correctly.
NOUS_ENGINE_API void LogOutputMultiline(LogLevel level, LogChannel channel, const char* text);
NOUS_ENGINE_API void LogOutputMultilineEx(LogLevel level, LogChannel channel,
                                          const char* file, int line, const char* function,
                                          const char* text);

// ──────────────────────────────────────────────────────────────────────────────
// Callback — invoked from the flush thread after each entry is committed.
// Keep at most one callback registered (replaces the previous one).
// ──────────────────────────────────────────────────────────────────────────────

NOUS_ENGINE_API void SetLogCallback(std::function<void(LogLevel, LogChannel, double, const char*)> callback);

// ──────────────────────────────────────────────────────────────────────────────
// History — cursor-based pull API (avoids full-copy on every call)
//
//   uint64_t cursor = 0;
//   std::vector<LogEntry> entries;
//   cursor = GetLogEntriesSince(cursor, entries);   // get everything so far
//   // ... each frame:
//   entries.clear();
//   cursor = GetLogEntriesSince(cursor, entries);   // get only new entries
// ──────────────────────────────────────────────────────────────────────────────

// Total entries ever committed to the ring buffer (monotonic, never decremented by ClearLogHistory).
NOUS_ENGINE_API uint64_t GetLogEntryCount();

// Appends all ring-buffer entries written at index >= fromIndex into outEntries.
// If fromIndex is older than the ring's oldest surviving entry, starts from the oldest available.
// Returns the new cursor value to pass on the next call.
NOUS_ENGINE_API uint64_t GetLogEntriesSince(uint64_t fromIndex, std::vector<LogEntry>& outEntries);

// Legacy full-copy API — preserved for backward compatibility.
NOUS_ENGINE_API std::deque<std::tuple<LogLevel, LogChannel, double, std::string>> GetLogHistory();
NOUS_ENGINE_API void ClearLogHistory();

// ──────────────────────────────────────────────────────────────────────────────
// Runtime controls
// ──────────────────────────────────────────────────────────────────────────────

NOUS_ENGINE_API void SetLogLevelEnabled(LogLevel level, bool enabled);
NOUS_ENGINE_API bool IsLogLevelEnabled(LogLevel level);

NOUS_ENGINE_API void SetLoggingPaused(bool paused);
NOUS_ENGINE_API bool IsLoggingPaused();

// ──────────────────────────────────────────────────────────────────────────────
// Macros
// ──────────────────────────────────────────────────────────────────────────────

// ── Source-location helper ────────────────────────────────────────────────────
// Resolves __FILE__ to just the filename at compile time (no runtime cost).
// Walks the string literal backwards to find the last path separator.
namespace NousLogDetail {
    constexpr const char* SourceFileName(const char* path) noexcept {
        const char* name = path;
        for (const char* p = path; *p; ++p)
            if (*p == '/' || *p == '\\') name = p + 1;
        return name;
    }
}
#define NOUS_SOURCE_FILE NousLogDetail::SourceFileName(__FILE__)

// ── Level macros ──────────────────────────────────────────────────────────────

#ifndef NOUS_FATAL
#define NOUS_FATAL(message, ...) LogOutputEx(LogLevel::LOG_LEVEL_FATAL, LogChannel::DEFAULT, NOUS_SOURCE_FILE, __LINE__, __FUNCTION__, message, ##__VA_ARGS__)
#endif

#ifndef NOUS_ERROR
#define NOUS_ERROR(message, ...) LogOutputEx(LogLevel::LOG_LEVEL_ERROR, LogChannel::DEFAULT, NOUS_SOURCE_FILE, __LINE__, __FUNCTION__, message, ##__VA_ARGS__)
#endif

#if LOG_WARN_ENABLED == 1
#define NOUS_WARN(message, ...) LogOutputEx(LogLevel::LOG_LEVEL_WARN, LogChannel::DEFAULT, NOUS_SOURCE_FILE, __LINE__, __FUNCTION__, message, ##__VA_ARGS__)
#else
#define NOUS_WARN(message, ...)
#endif

#if LOG_INFO_ENABLED == 1
#define NOUS_INFO(message, ...) LogOutputEx(LogLevel::LOG_LEVEL_INFO, LogChannel::DEFAULT, NOUS_SOURCE_FILE, __LINE__, __FUNCTION__, message, ##__VA_ARGS__)
#else
#define NOUS_INFO(message, ...)
#endif

#if LOG_DEBUG_ENABLED == 1
#define NOUS_DEBUG(message, ...) LogOutputEx(LogLevel::LOG_LEVEL_DEBUG, LogChannel::DEFAULT, NOUS_SOURCE_FILE, __LINE__, __FUNCTION__, message, ##__VA_ARGS__)
#else
#define NOUS_DEBUG(message, ...)
#endif

#if LOG_TRACE_ENABLED == 1
#define NOUS_TRACE(message, ...) LogOutputEx(LogLevel::LOG_LEVEL_TRACE, LogChannel::DEFAULT, NOUS_SOURCE_FILE, __LINE__, __FUNCTION__, message, ##__VA_ARGS__)
#else
#define NOUS_TRACE(message, ...)
#endif

// ── Channel-scoped macros ─────────────────────────────────────────────────────

#define NOUS_LOG_CHANNEL(CHANNEL, LEVEL, message, ...) \
    LogOutputEx(LEVEL, CHANNEL, NOUS_SOURCE_FILE, __LINE__, __FUNCTION__, message, ##__VA_ARGS__)

#define NOUS_TRACE_C(CHANNEL, message, ...) NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_TRACE, message, ##__VA_ARGS__)
#define NOUS_DEBUG_C(CHANNEL, message, ...) NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_DEBUG, message, ##__VA_ARGS__)
#define NOUS_INFO_C(CHANNEL, message, ...)  NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_INFO,  message, ##__VA_ARGS__)
#define NOUS_WARN_C(CHANNEL, message, ...)  NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_WARN,  message, ##__VA_ARGS__)
#define NOUS_ERROR_C(CHANNEL, message, ...) NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_ERROR, message, ##__VA_ARGS__)
#define NOUS_FATAL_C(CHANNEL, message, ...) NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_FATAL, message, ##__VA_ARGS__)

// Multiline variant — splits text on '\n', each line gets full source location.
#define NOUS_MULTILINE_C(LEVEL, CHANNEL, text) \
    LogOutputMultilineEx(LEVEL, CHANNEL, NOUS_SOURCE_FILE, __LINE__, __FUNCTION__, text)

#endif // LOGGER_H
