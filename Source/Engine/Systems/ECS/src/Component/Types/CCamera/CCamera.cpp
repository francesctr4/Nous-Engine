#include <ECS/Component/Types/CCamera/CCamera.h>

#include <ECS/ComponentServices.h>
#include <ECS/Scene/iSceneHost.h>
#include <ECS/Component/Types/CTransform/CTransform.h>
#include <ECS/GameObject.h>
#include <CameraSystem/Camera.h>

#include <glm/gtc/matrix_transform.hpp>
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

// ---------------------------------------------------------------------------
// Matrix computation
// ---------------------------------------------------------------------------

glm::mat4 CCamera::GetViewMatrix() const
{
    auto go = GetGameObject();
    auto* transform = go.IsValid() ? go.TryGetComponent<CTransform>() : nullptr;
    if (!transform)
        return glm::mat4(1.0f);

    const glm::vec3 pos     = transform->position;
    const glm::vec3 forward = transform->GetForward();
    const glm::vec3 up      = transform->GetUp();
    return glm::lookAt(pos, pos + forward, up);
}

glm::mat4 CCamera::GetProjectionMatrix() const
{
    const float vfovRad = glm::radians(fov);
    return glm::perspective(vfovRad, aspectRatio, nearPlane, farPlane);
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void CCamera::OnUpdate(float /*deltaTime*/)
{
    if (!isMainCamera)
        return;

    auto go = GetGameObject();
    if (!go.IsValid()) return;

    auto* transform = go.TryGetComponent<CTransform>();
    if (!transform)
        return;

    const ComponentServices& s = Services();
    if (!s.host) return;

    // Always derive aspect ratio from the actual window so it's correct on any display.
    // Assigned BEFORE the game-camera null check so it stays correct in a scene that
    // has no game camera assigned — GetProjectionMatrix() reads it either way.
    aspectRatio = s.host->GetWindowAspect();

    // May legitimately be null (no game camera in this scene). The pre-seam code
    // dereferenced this unconditionally and crashed; only isMainCamera defaulting
    // to false kept it hidden.
    Camera* gameCamera = s.host->GetGameCamera();
    if (!gameCamera) return;

    gameCamera->SetPos(transform->position);
    gameCamera->SetFront(transform->GetForward());
    gameCamera->SetUp(transform->GetUp());
    gameCamera->SetVerticalFOV(fov);
    gameCamera->SetNearPlane(nearPlane);
    gameCamera->SetFarPlane(farPlane);
    gameCamera->SetAspectRatio(aspectRatio);
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

JsonObject CCamera::Serialize() const
{
    JsonObject root;
    root.Set("type",         GetType());
    root.Set("fov",          fov);
    root.Set("nearPlane",    nearPlane);
    root.Set("farPlane",     farPlane);
    root.Set("aspectRatio",  aspectRatio);
    root.Set("isMainCamera", isMainCamera);
    return root;
}

void CCamera::Deserialize(const JsonObject& obj)
{
    fov          = obj.GetFloat("fov",          fov);
    nearPlane    = obj.GetFloat("nearPlane",    nearPlane);
    farPlane     = obj.GetFloat("farPlane",     farPlane);
    aspectRatio  = obj.GetFloat("aspectRatio",  aspectRatio);
    isMainCamera = obj.GetBool ("isMainCamera", isMainCamera);
}
