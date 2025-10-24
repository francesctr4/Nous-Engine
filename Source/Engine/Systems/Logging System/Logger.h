#ifndef LOGGER_H
#define LOGGER_H

#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"
#include "LogChannel.h"

#include <vector>
#include <string>
#include <functional>
#include <deque>

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
	LOG_LEVEL_WARN = 2,
	LOG_LEVEL_INFO = 3,

	// Exclusively on Debug Build
	LOG_LEVEL_DEBUG = 4,
	LOG_LEVEL_TRACE = 5,

	LOG_LEVEL_MAX

} LogLevel;

NOUS_ENGINE_API bool InitializeLogging();
NOUS_ENGINE_API void ShutdownLogging();

NOUS_ENGINE_API void AppendToLogFile(const char* message);

NOUS_ENGINE_API void LogOutput(LogLevel level, const char* message, ...);
NOUS_ENGINE_API void LogOutput(LogLevel level, LogChannel channel, const char* message, ...);

NOUS_ENGINE_API void SetLogCallback(std::function<void(LogLevel, LogChannel, double, const char*)> callback);
NOUS_ENGINE_API std::deque<std::tuple<LogLevel, LogChannel, double, std::string>> GetLogHistory();
NOUS_ENGINE_API void ClearLogHistory();

NOUS_ENGINE_API void SetLogLevelEnabled(LogLevel level, bool enabled);
NOUS_ENGINE_API bool IsLogLevelEnabled(LogLevel level);

NOUS_ENGINE_API void SetLoggingPaused(bool paused);
NOUS_ENGINE_API bool IsLoggingPaused();

#ifndef NOUS_FATAL
// Logs a fatal-level message.
#define NOUS_FATAL(message, ...) LogOutput(LogLevel::LOG_LEVEL_FATAL, message, ##__VA_ARGS__)
#endif // NOUS_FATAL

#ifndef NOUS_ERROR
// Logs a error-level message.
#define NOUS_ERROR(message, ...) LogOutput(LogLevel::LOG_LEVEL_ERROR, message, ##__VA_ARGS__)
#endif // NOUS_ERROR

#if LOG_WARN_ENABLED == 1
// Logs a warning-level message.
#define NOUS_WARN(message, ...) LogOutput(LogLevel::LOG_LEVEL_WARN, message, ##__VA_ARGS__)
#else
// Does nothing when LOG_WARN_ENABLED != 1
#define NOUS_WARN(message, ...)
#endif

#if LOG_INFO_ENABLED == 1
// Logs a info-level message.
#define NOUS_INFO(message, ...) LogOutput(LogLevel::LOG_LEVEL_INFO, message, ##__VA_ARGS__)
#else
// Does nothing when LOG_INFO_ENABLED != 1
#define NOUS_INFO(message, ...)
#endif

#if LOG_DEBUG_ENABLED == 1
// Logs a debug-level message.
#define NOUS_DEBUG(message, ...) LogOutput(LogLevel::LOG_LEVEL_DEBUG, message, ##__VA_ARGS__)
#else 
// Does nothing when LOG_DEBUG_ENABLED != 1
#define NOUS_DEBUG(message, ...)
#endif

#if LOG_TRACE_ENABLED == 1
// Logs a trace-level message.
#define NOUS_TRACE(message, ...) LogOutput(LogLevel::LOG_LEVEL_TRACE, message, ##__VA_ARGS__)
#else
// Does nothing when LOG_TRACE_ENABLED != 1
#define NOUS_TRACE(message, ...)
#endif

#define NOUS_LOG_CHANNEL(CHANNEL, LEVEL, message, ...) \
    LogOutput(LEVEL, CHANNEL, message, ##__VA_ARGS__)

#define NOUS_TRACE_C(CHANNEL, message, ...) NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_TRACE, message, ##__VA_ARGS__)
#define NOUS_DEBUG_C(CHANNEL, message, ...) NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_DEBUG, message, ##__VA_ARGS__)
#define NOUS_INFO_C(CHANNEL, message, ...)  NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_INFO,  message, ##__VA_ARGS__)
#define NOUS_WARN_C(CHANNEL, message, ...)  NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_WARN,  message, ##__VA_ARGS__)
#define NOUS_ERROR_C(CHANNEL, message, ...) NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_ERROR, message, ##__VA_ARGS__)
#define NOUS_FATAL_C(CHANNEL, message, ...) NOUS_LOG_CHANNEL(CHANNEL, LogLevel::LOG_LEVEL_FATAL, message, ##__VA_ARGS__)

#endif // LOGGER_H