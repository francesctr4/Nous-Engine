#include "Scripting System/ScriptRegistry.h"
#include "ScriptRegistryExport.h"

extern "C" SCRIPTS_API inline ScriptRegistry* GetScriptRegistry()
{
    return &g_ScriptRegistry;
}
