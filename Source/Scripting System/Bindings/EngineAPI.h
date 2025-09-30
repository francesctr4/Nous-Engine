#ifndef NOUS_ENGINE_ENGINEAPI_H
#define NOUS_ENGINE_ENGINEAPI_H

#include <functional>

// --- Subsystem APIs ---
struct LoggerAPI
{
    // Function pointers exposed to scripts
    void (*Trace)(const char* msg) = nullptr;
    void (*Debug)(const char* msg) = nullptr;
    void (*Info)(const char* msg) = nullptr;
    void (*Warn)(const char* msg) = nullptr;
    void (*Error)(const char* msg) = nullptr;
    void (*Fatal)(const char* msg) = nullptr;
};

struct InputAPI
{
    void (*GetKey)(int id) = nullptr;
};

// --- Engine API (root) ---
struct EngineAPI
{
    LoggerAPI Logger;
    InputAPI Input;
    // Add PhysicsAPI, InputAPI, etc here later
};

// Only declare here
extern EngineAPI* Nous_Engine;

// Exported function to set the pointer
extern "C" __declspec(dllexport) void __cdecl SetEngineAPI(EngineAPI* api);

#endif //NOUS_ENGINE_ENGINEAPI_H
