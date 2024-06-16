#pragma once

#include "Globals.h"

#define LOG_WARN_ENABLED 1
#define LOG_INFO_ENABLED 1
#define LOG_DEBUG_ENABLED 1
#define LOG_TRACE_ENABLED 1

#ifndef _DEBUG
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

	ALL

} LogLevel;

bool InitializeLogging();
void ShutdownLogging();

void LogOutput(LogLevel level, const char* message, ...);

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