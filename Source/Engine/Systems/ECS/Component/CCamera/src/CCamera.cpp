#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"

#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/CameraSystem/Camera/include/Camera.h"

#include <glm/gtc/matrix_transform.hpp>
#include <parson.h>

// ---------------------------------------------------------------------------
// Matrix computation
// ---------------------------------------------------------------------------

glm::mat4 CCamera::GetViewMatrix() const
{
    auto* transform = GetGameObject()->TryGetComponent<CTransform>();
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

    auto* transform = GetGameObject()->TryGetComponent<CTransform>();
    if (!transform)
        return;

    Camera* gameCamera = External->GetScene()->gameCamera;
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

JSON_Value* CCamera::Serialize() const
{
    JSON_Value*  objVal = json_value_init_object();
    JSON_Object* obj    = json_value_get_object(objVal);

    json_object_set_string(obj, "type",          GetType().c_str());
    json_object_set_number(obj, "fov",            fov);
    json_object_set_number(obj, "nearPlane",      nearPlane);
    json_object_set_number(obj, "farPlane",       farPlane);
    json_object_set_number(obj, "aspectRatio",    aspectRatio);
    json_object_set_boolean(obj, "isMainCamera",  isMainCamera);

    return objVal;
}

void CCamera::Deserialize(JSON_Object* obj)
{
    if (json_object_has_value(obj, "fov"))
        fov         = static_cast<float>(json_object_get_number(obj, "fov"));
    if (json_object_has_value(obj, "nearPlane"))
        nearPlane   = static_cast<float>(json_object_get_number(obj, "nearPlane"));
    if (json_object_has_value(obj, "farPlane"))
        farPlane    = static_cast<float>(json_object_get_number(obj, "farPlane"));
    if (json_object_has_value(obj, "aspectRatio"))
        aspectRatio = static_cast<float>(json_object_get_number(obj, "aspectRatio"));
    if (json_object_has_value(obj, "isMainCamera"))
        isMainCamera = json_object_get_boolean(obj, "isMainCamera") != 0;
}
