#ifndef NOUS_ENGINE_CPP_LOGGER_BINDINGS_H
#define NOUS_ENGINE_CPP_LOGGER_BINDINGS_H

#ifdef __cplusplus
extern "C" {
#endif

// Expose basic logging functions to scripts
__declspec(dllexport) void Script_LogInfo(const char* message);
__declspec(dllexport) void Script_LogWarning(const char* message);
__declspec(dllexport) void Script_LogError(const char* message);
__declspec(dllexport) void Script_LogFatal(const char* message);
__declspec(dllexport) void Script_LogDebug(const char* message);
__declspec(dllexport) void Script_LogTrace(const char* message);

#ifdef __cplusplus
}
#endif

#endif //NOUS_ENGINE_CPP_LOGGER_BINDINGS_H
