#include <Engine/Scripting/EngineAPI/Bindings/Logger/LoggerBindings.h>

#include <Logger/Logger.h>

#include <cstdarg>
#include <cstdio>

static void ScriptTrace(const char* msg, ...)
{
    char buffer[1024];

    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);

    NOUS_TRACE("%s", buffer);
}

static void ScriptDebug(const char* msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);

    NOUS_DEBUG("%s", buffer);
}

static void ScriptInfo(const char* msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);

    NOUS_INFO("%s", buffer);
}

static void ScriptWarn(const char* msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);

    NOUS_WARN("%s", buffer);
}

static void ScriptError(const char* msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);

    NOUS_ERROR("%s", buffer);
}

static void ScriptFatal(const char* msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);

    NOUS_FATAL("%s", buffer);
}

// ── Source-location variants ──────────────────────────────────────────────────
// file/line/func come from the script's call site via NOUS_SCRIPT_* macros.
// LogOutputEx is called directly to avoid the macros re-capturing __FILE__ here.

static void ScriptTraceEx(const char* file, int line, const char* func, const char* msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);
    LogOutputEx(LogLevel::LOG_LEVEL_TRACE, LogChannel::NOUS_ENGINE_SCRIPTING_USER_SCRIPTS, file, line, func, "%s", buffer);
}

static void ScriptDebugEx(const char* file, int line, const char* func, const char* msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);
    LogOutputEx(LogLevel::LOG_LEVEL_DEBUG, LogChannel::NOUS_ENGINE_SCRIPTING_USER_SCRIPTS, file, line, func, "%s", buffer);
}

static void ScriptInfoEx(const char* file, int line, const char* func, const char* msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);
    LogOutputEx(LogLevel::LOG_LEVEL_INFO, LogChannel::NOUS_ENGINE_SCRIPTING_USER_SCRIPTS, file, line, func, "%s", buffer);
}

static void ScriptWarnEx(const char* file, int line, const char* func, const char* msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);
    LogOutputEx(LogLevel::LOG_LEVEL_WARN, LogChannel::NOUS_ENGINE_SCRIPTING_USER_SCRIPTS, file, line, func, "%s", buffer);
}

static void ScriptErrorEx(const char* file, int line, const char* func, const char* msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);
    LogOutputEx(LogLevel::LOG_LEVEL_ERROR, LogChannel::NOUS_ENGINE_SCRIPTING_USER_SCRIPTS, file, line, func, "%s", buffer);
}

static void ScriptFatalEx(const char* file, int line, const char* func, const char* msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);
    LogOutputEx(LogLevel::LOG_LEVEL_FATAL, LogChannel::NOUS_ENGINE_SCRIPTING_USER_SCRIPTS, file, line, func, "%s", buffer);
}

void SetupLoggerBindings(LoggerAPI& logger)
{
    logger.Trace = &ScriptTrace;
    logger.Debug = &ScriptDebug;
    logger.Info  = &ScriptInfo;
    logger.Warn  = &ScriptWarn;
    logger.Error = &ScriptError;
    logger.Fatal = &ScriptFatal;

    logger.TraceEx = &ScriptTraceEx;
    logger.DebugEx = &ScriptDebugEx;
    logger.InfoEx  = &ScriptInfoEx;
    logger.WarnEx  = &ScriptWarnEx;
    logger.ErrorEx = &ScriptErrorEx;
    logger.FatalEx = &ScriptFatalEx;
}
