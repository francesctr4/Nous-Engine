#include "Engine/Scripting/EngineAPI/Bindings/Component/ComponentBindings.h"

#include "Engine/Scripting/iScriptSceneHost.h"
#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CLight.h>
#include <ECS/Component/Types/CCamera.h>
#include <ECS/Component/Types/CMesh.h>
#include <ECS/Component/Types/CScript.h>
#include <Logger/Logger.h>

static IScriptSceneHost* s_scene = nullptr;

static GameObject ResolveGO(uint32_t id)
{
    if (!s_scene || !s_scene->GetActiveScene()) return {};
    return s_scene->GetActiveScene()->GetGameObjectByID(id);
}

void SetupComponentBindings(ComponentAPI& component, IScriptSceneHost* sceneHost)
{
    s_scene = sceneHost;

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
