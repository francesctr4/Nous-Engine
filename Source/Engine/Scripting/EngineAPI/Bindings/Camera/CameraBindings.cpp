#include "Engine/Scripting/EngineAPI/Bindings/Camera/CameraBindings.h"

#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"
#include "Engine/Core/Logger/Logger.h"

static ModuleScene* s_scene = nullptr;

static CCamera* GetCamera(uint32_t goId)
{
    if (!s_scene || !s_scene->activeScene) return nullptr;
    GameObject go = s_scene->activeScene->GetGameObjectByID(goId);
    if (!go.IsValid()) { NOUS_WARN("[CameraAPI] GameObject %u not found", goId); return nullptr; }
    if (!go.HasComponent<CCamera>()) { NOUS_WARN("[CameraAPI] GameObject %u has no CCamera", goId); return nullptr; }
    return &go.GetComponent<CCamera>();
}

void SetupCameraBindings(CameraAPI& camera, ModuleScene* moduleScene)
{
    s_scene = moduleScene;

    camera.GetMainCamera = []() -> uint32_t {
        if (!s_scene || !s_scene->activeScene) return 0;
        for (GameObject& go : s_scene->activeScene->GetGameObjects()) {
            if (go.HasComponent<CCamera>() && go.GetComponent<CCamera>().isMainCamera)
                return go.GetID();
        }
        return 0;
    };

    camera.GetFOV = [](uint32_t goId) -> float {
        const CCamera* cam = GetCamera(goId);
        return cam ? cam->fov : 0.0f;
    };

    camera.SetFOV = [](uint32_t goId, float fov) {
        CCamera* cam = GetCamera(goId);
        if (cam) cam->fov = fov;
    };

    camera.GetNear = [](uint32_t goId) -> float {
        const CCamera* cam = GetCamera(goId);
        return cam ? cam->nearPlane : 0.0f;
    };

    camera.SetNear = [](uint32_t goId, float nearPlane) {
        CCamera* cam = GetCamera(goId);
        if (cam) cam->nearPlane = nearPlane;
    };

    camera.GetFar = [](uint32_t goId) -> float {
        const CCamera* cam = GetCamera(goId);
        return cam ? cam->farPlane : 0.0f;
    };

    camera.SetFar = [](uint32_t goId, float farPlane) {
        CCamera* cam = GetCamera(goId);
        if (cam) cam->farPlane = farPlane;
    };

    camera.IsMain = [](uint32_t goId) -> int {
        const CCamera* cam = GetCamera(goId);
        return cam ? (cam->isMainCamera ? 1 : 0) : 0;
    };

    camera.SetMain = [](uint32_t goId, int isMain) {
        CCamera* cam = GetCamera(goId);
        if (cam) cam->isMainCamera = (isMain != 0);
    };
}
