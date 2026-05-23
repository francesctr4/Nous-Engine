#include "Engine/Scripting/EngineAPI/Bindings/Component/ComponentBindings.h"

#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CLight/include/CLight.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/CScript/include/CScript.h"
#include "Engine/Core/Logger/Logger.h"

static ModuleScene* s_scene = nullptr;

static GameObject ResolveGO(uint32_t id)
{
    if (!s_scene || !s_scene->activeScene) return {};
    return s_scene->activeScene->GetGameObjectByID(id);
}

void SetupComponentBindings(ComponentAPI& component, ModuleScene* moduleScene)
{
    s_scene = moduleScene;

    component.HasLight = [](uint32_t goId) -> bool {
        GameObject go = ResolveGO(goId);
        return go.IsValid() && go.HasComponent<CLight>();
    };

    component.HasCamera = [](uint32_t goId) -> bool {
        GameObject go = ResolveGO(goId);
        return go.IsValid() && go.HasComponent<CCamera>();
    };

    component.HasMesh = [](uint32_t goId) -> bool {
        GameObject go = ResolveGO(goId);
        return go.IsValid() && go.HasComponent<CMesh>();
    };

    component.HasScript = [](uint32_t goId) -> bool {
        GameObject go = ResolveGO(goId);
        return go.IsValid() && go.HasComponent<CScript>();
    };
}
