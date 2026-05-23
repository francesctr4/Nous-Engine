#ifndef NOUS_ENGINE_ENGINEAPI_H
#define NOUS_ENGINE_ENGINEAPI_H

// Forward declarations
struct LoggerAPI;
struct InputAPI;
struct TimeAPI;
struct GameObjectAPI;
struct ComponentAPI;
struct LightAPI;
struct MaterialAPI;
struct CameraAPI;
struct SceneAPI;

// --- Engine API (root) ---
struct EngineAPI
{
    LoggerAPI*     Logger;
    InputAPI*      Input;
    TimeAPI*       Time;
    GameObjectAPI* GameObject;
    ComponentAPI*  Component;
    LightAPI*      Light;
    MaterialAPI*   Material;
    CameraAPI*     Camera;
    SceneAPI*      Scene;
};

// Only declare here
extern EngineAPI* Nous_Engine;

// Platform-specific export/calling convention for the scripting DLL boundary
#ifdef _WIN32
  #define NOUS_SCRIPT_EXPORT __declspec(dllexport)
  #define NOUS_SCRIPT_CDECL  __cdecl
#else
  #define NOUS_SCRIPT_EXPORT __attribute__((visibility("default")))
  #define NOUS_SCRIPT_CDECL
#endif

// Exported function to set the pointer
extern "C" NOUS_SCRIPT_EXPORT void NOUS_SCRIPT_CDECL SetEngineAPI(EngineAPI* api);

#endif //NOUS_ENGINE_ENGINEAPI_H
