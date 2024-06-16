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

enum class LogLevel {

	NONE = -1,

	LOG_LEVEL_FATAL = 0,
	LOG_LEVEL_ERROR = 1,
	LOG_LEVEL_WARN = 2,
	LOG_LEVEL_INFO = 3,

	// Exclusively on Debug Build
	LOG_LEVEL_DEBUG = 4,
	LOG_LEVEL_TRACE = 5,

	ALL
};

bool InitializeLogging();
void ShutdownLogging();

void LogOutput(LogLevel level, const char* message, ...);

// Logs a fatal-level message.
#define KFATAL(message, ...) LogOutput(LogLevel::LOG_LEVEL_FATAL, message, ##__VA_ARGS__)

// Logs a fatal-level message.
#define KFATAL(message, ...) LogOutput(LogLevel::LOG_LEVEL_FATAL, message, ##__VA_ARGS__)