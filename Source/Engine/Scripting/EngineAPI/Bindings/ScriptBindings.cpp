#include <Engine/Scripting/EngineAPI/Bindings/ScriptBindings.h>

#include <Engine/Scripting/EngineAPI/EngineAPI.h>
#include <Engine/Core/MemoryManager/MemoryManager.h>

void ScriptBindings::InitializeBindings(EngineAPI*& api)
{
    api = NOUS_NEW<EngineAPI>(MemoryTag::SCRIPTING_SYSTEM);
    api->Logger     = NOUS_NEW<LoggerAPI>    (MemoryTag::SCRIPTING_SYSTEM);
    api->Input      = NOUS_NEW<InputAPI>     (MemoryTag::SCRIPTING_SYSTEM);
    api->GameObject = NOUS_NEW<GameObjectAPI>(MemoryTag::SCRIPTING_SYSTEM);
    api->Light      = NOUS_NEW<LightAPI>     (MemoryTag::SCRIPTING_SYSTEM);
    api->Material   = NOUS_NEW<MaterialAPI>  (MemoryTag::SCRIPTING_SYSTEM);
}

void ScriptBindings::SetupAllBindings(EngineAPI& api, ModuleInput* moduleInput, ModuleScene* moduleScene)
{
    SetupLoggerBindings    (*api.Logger);
    SetupInputBindings     (*api.Input,      moduleInput);
    SetupGameObjectBindings(*api.GameObject, moduleScene);
    SetupLightBindings     (*api.Light,      moduleScene);
    SetupMaterialBindings  (*api.Material,   moduleScene);
}

void ScriptBindings::DeleteBindings(EngineAPI*& api)
{
    if (!api) return;
    NOUS_DELETE(api->Material,   MemoryTag::SCRIPTING_SYSTEM);
    NOUS_DELETE(api->Light,      MemoryTag::SCRIPTING_SYSTEM);
    NOUS_DELETE(api->GameObject, MemoryTag::SCRIPTING_SYSTEM);
    NOUS_DELETE(api->Input,      MemoryTag::SCRIPTING_SYSTEM);
    NOUS_DELETE(api->Logger,     MemoryTag::SCRIPTING_SYSTEM);
    NOUS_DELETE(api, MemoryTag::SCRIPTING_SYSTEM);
    api = nullptr;
}


