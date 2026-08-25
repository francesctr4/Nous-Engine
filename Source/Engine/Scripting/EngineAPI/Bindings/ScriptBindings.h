#ifndef NOUS_ENGINE_SCRIPTBINDINGS_H
#define NOUS_ENGINE_SCRIPTBINDINGS_H

// Include all subsystem API headers
#include <Engine/Scripting/EngineAPI/Bindings/Logger/LoggerBindings.h>
#include <Engine/Scripting/EngineAPI/Bindings/Input/InputBindings.h>
#include <Engine/Scripting/EngineAPI/Bindings/Time/TimeBindings.h>
#include <Engine/Scripting/EngineAPI/Bindings/GameObject/GameObjectBindings.h>
#include <Engine/Scripting/EngineAPI/Bindings/Component/ComponentBindings.h>
#include <Engine/Scripting/EngineAPI/Bindings/Light/LightBindings.h>
#include <Engine/Scripting/EngineAPI/Bindings/Material/MaterialBindings.h>
#include <Engine/Scripting/EngineAPI/Bindings/Camera/CameraBindings.h>
#include <Engine/Scripting/EngineAPI/Bindings/Scene/SceneBindings.h>

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
