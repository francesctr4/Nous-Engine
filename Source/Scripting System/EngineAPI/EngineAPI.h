#ifndef NOUS_ENGINE_ENGINEAPI_H
#define NOUS_ENGINE_ENGINEAPI_H

// Forward declarations
struct LoggerAPI;
struct InputAPI;
struct GameObjectAPI;

// --- Engine API (root) ---
struct EngineAPI
{
    LoggerAPI* Logger;
    InputAPI* Input;
    GameObjectAPI* GameObject;
    // Add PhysicsAPI, InputAPI, etc here later
};

// Only declare here
extern EngineAPI* Nous_Engine;

// Exported function to set the pointer
extern "C" __declspec(dllexport) void __cdecl SetEngineAPI(EngineAPI* api);

#endif //NOUS_ENGINE_ENGINEAPI_H
