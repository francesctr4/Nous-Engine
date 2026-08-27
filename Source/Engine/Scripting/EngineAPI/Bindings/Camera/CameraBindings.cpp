#include "Engine/Scripting/EngineAPI/Bindings/Camera/CameraBindings.h"

#include "Engine/Scripting/iScriptSceneHost.h"
#include <ECS/Scene/Scene.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CCamera/CCamera.h>
#include <Logger/Logger.h>

static IScriptSceneHost* s_scene = nullptr;

static CCamera* GetCamera(uint32_t goId)
{
    if (!s_scene || !s_scene->GetActiveScene()) return nullptr;
    GameObject go = s_scene->GetActiveScene()->GetGameObjectByID(goId);
    if (!go.IsValid()) { NOUS_WARN("[CameraAPI] GameObject %u not found", goId); return nullptr; }
    if (!go.HasComponent<CCamera>()) { NOUS_WARN("[CameraAPI] GameObject %u has no CCamera", goId); return nullptr; }
    return &go.GetComponent<CCamera>();
}

void SetupCameraBindings(CameraAPI& camera, IScriptSceneHost* sceneHost)
{
    s_scene = sceneHost;

    camera.GetMainCamera = []() -> uint32_t {
        if (!s_scene || !s_scene->GetActiveScene()) return 0;
        for (GameObject& go : s_scene->GetActiveScene()->GetGameObjects()) {
            const CCamera* cam = go.TryGetComponent<CCamera>();
            if (cam && cam->isMainCamera)
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
