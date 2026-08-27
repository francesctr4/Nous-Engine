#include <Scripting/EngineAPI/Bindings/ScriptBindings.h>

#include <Scripting/EngineAPI/EngineAPI.h>
#include <MemoryManager/MemoryManager.h>

void ScriptBindings::InitializeBindings(EngineAPI*& api)
{
    api = NOUS_NEW<EngineAPI>(MemoryTag::SCRIPTING_SYSTEM);
    api->Logger     = NOUS_NEW<LoggerAPI>    (MemoryTag::SCRIPTING_SYSTEM);
    api->Input      = NOUS_NEW<InputAPI>     (MemoryTag::SCRIPTING_SYSTEM);
    api->Time       = NOUS_NEW<TimeAPI>      (MemoryTag::SCRIPTING_SYSTEM);
    api->GameObject = NOUS_NEW<GameObjectAPI>(MemoryTag::SCRIPTING_SYSTEM);
    api->Component  = NOUS_NEW<ComponentAPI> (MemoryTag::SCRIPTING_SYSTEM);
    api->Light      = NOUS_NEW<LightAPI>     (MemoryTag::SCRIPTING_SYSTEM);
    api->Material   = NOUS_NEW<MaterialAPI>  (MemoryTag::SCRIPTING_SYSTEM);
    api->Camera     = NOUS_NEW<CameraAPI>    (MemoryTag::SCRIPTING_SYSTEM);
    api->Scene      = NOUS_NEW<SceneAPI>     (MemoryTag::SCRIPTING_SYSTEM);
}

void ScriptBindings::SetupAllBindings(EngineAPI& api, IScriptInput* input, IScriptSceneHost* sceneHost)
{
    SetupLoggerBindings    (*api.Logger);
    SetupInputBindings     (*api.Input,      input);
    SetupTimeBindings      (*api.Time);
    SetupGameObjectBindings(*api.GameObject, sceneHost);
    SetupComponentBindings (*api.Component, sceneHost);
    SetupLightBindings     (*api.Light, sceneHost);
    SetupMaterialBindings  (*api.Material, sceneHost);
    SetupCameraBindings    (*api.Camera, sceneHost);
    SetupSceneBindings     (*api.Scene, sceneHost);
}

void ScriptBindings::DeleteBindings(EngineAPI*& api)
{
    if (!api) return;
    NOUS_DELETE(api->Scene,     MemoryTag::SCRIPTING_SYSTEM);
    NOUS_DELETE(api->Camera,    MemoryTag::SCRIPTING_SYSTEM);
    NOUS_DELETE(api->Material,  MemoryTag::SCRIPTING_SYSTEM);
    NOUS_DELETE(api->Light,     MemoryTag::SCRIPTING_SYSTEM);
    NOUS_DELETE(api->Component, MemoryTag::SCRIPTING_SYSTEM);
    NOUS_DELETE(api->GameObject,MemoryTag::SCRIPTING_SYSTEM);
    NOUS_DELETE(api->Time,      MemoryTag::SCRIPTING_SYSTEM);
    NOUS_DELETE(api->Input,     MemoryTag::SCRIPTING_SYSTEM);
    NOUS_DELETE(api->Logger,    MemoryTag::SCRIPTING_SYSTEM);
    NOUS_DELETE(api, MemoryTag::SCRIPTING_SYSTEM);
    api = nullptr;
}


