#include <Engine/Scripting/EngineAPI/Bindings/LoggerBindings.h>

#include "Engine/Core/Logging System/Logger.h"
#include <cstdarg>

void SetupLoggerBindings(LoggerAPI &logger)
{
    logger.Trace = [](const char* msg, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer), msg, args);
        va_end(args);
        NOUS_TRACE("[SCRIPT] %s", buffer);
    };

    logger.Debug = [](const char* msg, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer), msg, args);
        va_end(args);
        NOUS_DEBUG("[SCRIPT] %s", buffer);
    };

    logger.Info = [](const char* msg, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer), msg, args);
        va_end(args);
        NOUS_INFO("[SCRIPT] %s", buffer);
    };

    logger.Warn = [](const char* msg, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer), msg, args);
        va_end(args);
        NOUS_WARN("[SCRIPT] %s", buffer);
    };

    logger.Error = [](const char* msg, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer), msg, args);
        va_end(args);
        NOUS_ERROR("[SCRIPT] %s", buffer);
    };

    logger.Fatal = [](const char* msg, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, msg);
        vsnprintf(buffer, sizeof(buffer), msg, args);
        va_end(args);
        NOUS_FATAL("[SCRIPT] %s", buffer);
    };
}
