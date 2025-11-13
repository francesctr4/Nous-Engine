#include <Engine/Scripting/EngineAPI/Bindings/ScriptBindings.h>

#include <Engine/Scripting/EngineAPI/EngineAPI.h>
#include <Engine/Core/MemoryManager/MemoryManager.h>

void ScriptBindings::InitializeBindings(EngineAPI*& api)
{
    api = NOUS_NEW<EngineAPI>(MemoryTag::GAME);
    // Allocate each subsystem API
    api->Logger = NOUS_NEW<LoggerAPI>(MemoryTag::GAME);
    api->Input = NOUS_NEW<InputAPI>(MemoryTag::GAME);
    api->GameObject = NOUS_NEW<GameObjectAPI>(MemoryTag::GAME);
}

void ScriptBindings::SetupAllBindings(EngineAPI& api)
{
    // Each subsystem handles its own bindings
    SetupLoggerBindings(*api.Logger);
    SetupInputBindings(*api.Input);
    SetupGameObjectBindings(*api.GameObject);
}

void ScriptBindings::DeleteBindings(EngineAPI*& api)
{
    // Delete in reverse order
    NOUS_DELETE(api->GameObject, MemoryTag::GAME);
    NOUS_DELETE(api->Input, MemoryTag::GAME);
    NOUS_DELETE(api->Logger, MemoryTag::GAME);
    NOUS_DELETE(api, MemoryTag::GAME);
}


