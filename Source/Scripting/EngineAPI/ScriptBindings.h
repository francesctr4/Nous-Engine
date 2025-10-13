#ifndef NOUS_ENGINE_SCRIPTBINDINGS_H
#define NOUS_ENGINE_SCRIPTBINDINGS_H

// Include all subsystem API headers
#include "Scripting/EngineAPI/Bindings/LoggerBindings.h"
#include "Scripting/EngineAPI/Bindings/InputBindings.h"
#include "Scripting/EngineAPI/Bindings/GameObjectBindings.h"

struct EngineAPI;

class ScriptBindings
{
public:
    static void InitializeBindings(EngineAPI*& api);
    static void SetupAllBindings(EngineAPI& api);
    static void DeleteBindings(EngineAPI*& api);
};

#endif // NOUS_ENGINE_SCRIPTBINDINGS_H
