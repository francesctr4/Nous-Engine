#ifndef NOUS_ENGINE_SCRIPTBINDINGS_H
#define NOUS_ENGINE_SCRIPTBINDINGS_H

// Include all subsystem API headers
#include <Engine/Scripting/EngineAPI/Bindings/Logger/LoggerBindings.h>
#include <Engine/Scripting/EngineAPI/Bindings/Input/InputBindings.h>
#include <Engine/Scripting/EngineAPI/Bindings/GameObject/GameObjectBindings.h>
#include <Engine/Scripting/EngineAPI/Bindings/Light/LightBindings.h>
#include <Engine/Scripting/EngineAPI/Bindings/Material/MaterialBindings.h>

struct EngineAPI;
class ModuleInput;
class ModuleScene;

class ScriptBindings
{
public:
    static void InitializeBindings(EngineAPI*& api);
    static void SetupAllBindings(EngineAPI& api, ModuleInput* moduleInput, ModuleScene* moduleScene);
    static void DeleteBindings(EngineAPI*& api);
};

#endif // NOUS_ENGINE_SCRIPTBINDINGS_H
