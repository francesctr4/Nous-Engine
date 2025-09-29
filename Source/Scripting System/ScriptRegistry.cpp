#include "Scripting System/ScriptRegistry.h"

// function-local static ensures safe initialization
extern "C" SCRIPTS_API inline ScriptRegistry* GetScriptRegistry()
{
    static ScriptRegistry instance;
    return &instance;
}
