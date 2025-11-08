#include <Engine/Scripting/EngineAPI/ScriptBindings.h>

#include <Engine/Scripting/EngineAPI/EngineAPI.h>
#include <Engine/Core/Memory Manager/MemoryManager.h>

void ScriptBindings::InitializeBindings(EngineAPI*& api)
{
    api = NOUS_NEW<EngineAPI>(MemoryManager::MemoryTag::GAME);
    // Allocate each subsystem API
    api->Logger = NOUS_NEW<LoggerAPI>(MemoryManager::MemoryTag::GAME);
    api->Input = NOUS_NEW<InputAPI>(MemoryManager::MemoryTag::GAME);
    api->GameObject = NOUS_NEW<GameObjectAPI>(MemoryManager::MemoryTag::GAME);
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
    NOUS_DELETE(api->GameObject, MemoryManager::MemoryTag::GAME);
    NOUS_DELETE(api->Input, MemoryManager::MemoryTag::GAME);
    NOUS_DELETE(api->Logger, MemoryManager::MemoryTag::GAME);
    NOUS_DELETE(api, MemoryManager::MemoryTag::GAME);
}


