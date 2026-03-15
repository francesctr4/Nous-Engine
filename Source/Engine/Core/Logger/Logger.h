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
	LogLevel    level;
	LogChannel  channel;
	double      timestamp; // seconds since engine start (captured once on LogOutput call)
	std::string message;   // fully formatted: "[LEVEL]: text\n"
};

// ──────────────────────────────────────────────────────────────────────────────
// Lifecycle
// ──────────────────────────────────────────────────────────────────────────────

NOUS_ENGINE_API bool InitializeLogging();
NOUS_ENGINE_API void ShutdownLogging();

// ──────────────────────────────────────────────────────────────────────────────
// Log output
// ──────────────────────────────────────────────────────────────────────────────

// Direct file write, bypasses async queue — rarely needed externally.
NOUS_ENGINE_API void AppendToLogFile(const char* message);

NOUS_ENGINE_API void LogOutput(LogLevel level, const char* message, ...);
NOUS_ENGINE_API void LogOutput(LogLevel level, LogChannel channel, const char* message, ...);

// Splits `text` on '\n' and emits each non-empty line as a separate log entry.
// Use this for multi-line strings (e.g. stat dumps) so ConsoleWindow's
// per-line clipper renders them correctly.
NOUS_ENGINE_API void LogOutputMultiline(LogLevel level, LogChannel channel, const char* text);

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

#ifndef NOUS_FATAL
#define NOUS_FATAL(message, ...) LogOutput(LogLevel::LOG_LEVEL_FATAL, message, ##__VA_ARGS__)
#endif

#ifndef NOUS_ERROR
#define NOUS_ERROR(message, ...) LogOutput(LogLevel::LOG_LEVEL_ERROR, message, ##__VA_ARGS__)
#endif

#if LOG_WARN_ENABLED == 1
#define NOUS_WARN(message, ...) LogOutput(LogLevel::LOG_LEVEL_WARN, message, ##__VA_ARGS__)
#else
#define NOUS_WARN(message, ...)
#endif

#if LOG_INFO_ENABLED == 1
#define NOUS_INFO(message, ...) LogOutput(LogLevel::LOG_LEVEL_INFO, message, ##__VA_ARGS__)
#else
#define NOUS_INFO(message, ...)
#endif

#if LOG_DEBUG_ENABLED == 1
#define NOUS_DEBUG(message, ...) LogOutput(LogLevel::LOG_LEVEL_DEBUG, message, ##__VA_ARGS__)
#else
#define NOUS_DEBUG(message, ...)
#endif

#if LOG_TRACE_ENABLED == 1
#define NOUS_TRACE(message, ...) LogOutput(LogLevel::LOG_LEVEL_TRACE, message, ##__VA_ARGS__)
#else
#define NOUS_TRACE(message, ...)
#endif

// Channel-scoped macros
#define NOUS_LOG_CHANNEL(CHANNEL, LEVEL, message, ...) \
    LogOutput(LEVEL, CHANNEL, message, ##__VA_ARGS__)

#define NOUS_TRACE_C(CHANNEL, message, ...) NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_TRACE, message, ##__VA_ARGS__)
#define NOUS_DEBUG_C(CHANNEL, message, ...) NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_DEBUG, message, ##__VA_ARGS__)
#define NOUS_INFO_C(CHANNEL, message, ...)  NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_INFO,  message, ##__VA_ARGS__)
#define NOUS_WARN_C(CHANNEL, message, ...)  NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_WARN,  message, ##__VA_ARGS__)
#define NOUS_ERROR_C(CHANNEL, message, ...) NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_ERROR, message, ##__VA_ARGS__)
#define NOUS_FATAL_C(CHANNEL, message, ...) NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_FATAL, message, ##__VA_ARGS__)

#endif // LOGGER_H
