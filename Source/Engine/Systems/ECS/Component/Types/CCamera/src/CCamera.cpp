#include "Engine/Systems/ECS/Component/Types/CCamera/include/CCamera.h"

#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/Component/Types/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/CameraSystem/Camera/include/Camera.h"

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

    Scene* scene = go.GetScene();
    if (!scene) return;
    Camera* gameCamera = scene->GetModuleScene()->gameCamera;

    // Always derive aspect ratio from the actual window so it's correct on any display.
    aspectRatio = scene->GetModuleScene()->GetWindowAspect();

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
