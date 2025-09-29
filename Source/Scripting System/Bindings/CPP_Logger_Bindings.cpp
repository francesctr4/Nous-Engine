#include "CPP_Logger_Bindings.h"
#include "Utils/Logger.h"

void Script_LogInfo(const char* message) {
    NOUS_INFO("%s", message);
}

void Script_LogWarning(const char* message) {
    NOUS_WARN("%s", message);
}

void Script_LogError(const char* message) {
    NOUS_ERROR("%s", message);
}

void Script_LogFatal(const char* message) {
    NOUS_FATAL("%s", message);
}

void Script_LogDebug(const char* message) {
#if LOG_DEBUG_ENABLED == 1
    NOUS_DEBUG("%s", message);
#endif
}

void Script_LogTrace(const char* message) {
#if LOG_TRACE_ENABLED == 1
    NOUS_TRACE("%s", message);
#endif
}
