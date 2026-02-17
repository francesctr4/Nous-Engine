#include <Engine/Scripting/EngineAPI/Bindings/Logger/LoggerBindings.h>

#include "Engine/Core/Logger/Logger.h"

#include <cstdarg>
#include <cstdio>

static void ScriptTrace(const char* msg, ...)
{
    char buffer[1024];

    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);

    NOUS_TRACE("[SCRIPT] %s", buffer);
}

static void ScriptDebug(const char* msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);

    NOUS_DEBUG("[SCRIPT] %s", buffer);
}

static void ScriptInfo(const char* msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);

    NOUS_INFO("[SCRIPT] %s", buffer);
}

static void ScriptWarn(const char* msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);

    NOUS_WARN("[SCRIPT] %s", buffer);
}

static void ScriptError(const char* msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);

    NOUS_ERROR("[SCRIPT] %s", buffer);
}

static void ScriptFatal(const char* msg, ...)
{
    char buffer[1024];
    va_list args;
    va_start(args, msg);
    std::vsnprintf(buffer, sizeof(buffer), msg, args);
    va_end(args);

    NOUS_FATAL("[SCRIPT] %s", buffer);
}

void SetupLoggerBindings(LoggerAPI& logger)
{
    logger.Trace = &ScriptTrace;
    logger.Debug = &ScriptDebug;
    logger.Info  = &ScriptInfo;
    logger.Warn  = &ScriptWarn;
    logger.Error = &ScriptError;
    logger.Fatal = &ScriptFatal;
}
