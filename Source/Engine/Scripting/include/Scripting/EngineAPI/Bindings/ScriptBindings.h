#ifndef NOUS_ENGINE_SCRIPTBINDINGS_H
#define NOUS_ENGINE_SCRIPTBINDINGS_H

// Include all subsystem API headers
#include <Scripting/EngineAPI/Bindings/LoggerBindings.h>
#include <Scripting/EngineAPI/Bindings/InputBindings.h>
#include <Scripting/EngineAPI/Bindings/TimeBindings.h>
#include <Scripting/EngineAPI/Bindings/GameObjectBindings.h>
#include <Scripting/EngineAPI/Bindings/ComponentBindings.h>
#include <Scripting/EngineAPI/Bindings/LightBindings.h>
#include <Scripting/EngineAPI/Bindings/MaterialBindings.h>
#include <Scripting/EngineAPI/Bindings/CameraBindings.h>
#include <Scripting/EngineAPI/Bindings/SceneBindings.h>

struct EngineAPI;
class IScriptInput;
class IScriptSceneHost;

class ScriptBindings
{
public:
    static void InitializeBindings(EngineAPI*& api);
    static void SetupAllBindings(EngineAPI& api, IScriptInput* input, IScriptSceneHost* sceneHost);
    static void DeleteBindings(EngineAPI*& api);
};

#endif // NOUS_ENGINE_SCRIPTBINDINGS_H
