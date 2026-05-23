#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

glm::mat4 CTransform::GetLocalMatrix() const {
    glm::mat4 T = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 R = glm::toMat4(orientation);
    glm::mat4 S = glm::scale(glm::mat4(1.0f), scale);
    return T * R * S;
}

void CTransform::SetEulerRotation(const glm::vec3& eulerDegrees) {
    eulerHint = eulerDegrees;
    // Build quaternion using GLM's default Euler convention — must match GetEulerAngles()
    // so that the round-trip SetEulerRotation(GetEulerAngles()) is lossless
    orientation = glm::quat(glm::radians(eulerDegrees));
    MarkDirty();
}

void CTransform::Rotate(const glm::quat& deltaRotation) {
    orientation = glm::normalize(deltaRotation * orientation);
    eulerHint = GetEulerAngles();
    MarkDirty();
}

glm::vec3 CTransform::GetEulerAngles() const {
    // Extract Euler angles from quaternion in degrees
    // Uses GLM's built-in extraction (pitch=X, yaw=Y, roll=Z)
    return glm::degrees(glm::eulerAngles(orientation));
}

glm::vec3 CTransform::GetForward() const {
    return glm::normalize(orientation * glm::vec3(0.0f, 0.0f, -1.0f));
}

glm::vec3 CTransform::GetRight() const {
    return glm::normalize(orientation * glm::vec3(1.0f, 0.0f, 0.0f));
}

glm::vec3 CTransform::GetUp() const {
    return glm::normalize(orientation * glm::vec3(0.0f, 1.0f, 0.0f));
}

void CTransform::UpdateMatrix() {
    const glm::mat4 local = GetLocalMatrix();

    // If this component has a parent GO with a CTransform, chain the parent's
    // already-computed worldMatrix so children inherit parent transforms.
    {
        GameObject self = GetGameObject();
        if (self.IsValid())
        {
            GameObject parent = self.GetParent();
            if (parent.IsValid())
            {
                if (CTransform* pt = parent.TryGetComponent<CTransform>())
                {
                    worldMatrix = pt->worldMatrix * local;
                    return;
                }
            }
        }
    }

    worldMatrix = local;
}

JsonObject CTransform::Serialize() const {
    JsonObject root;
    root.Set("type",        GetType());
    root.Set("position",    position);
    root.Set("orientation", orientation);  // [w, x, y, z]
    root.Set("rotation",    eulerHint);    // hint for readability and backward compat
    root.Set("scale",       scale);
    return root;
}

void CTransform::Deserialize(const JsonObject& obj) {
    position = obj.GetVec3("position", position);

    // Orientation quaternion — preferred; fall back to Euler hint for old scenes
    if (obj.HasKey("orientation")) {
        orientation = obj.GetQuat("orientation", orientation);
        eulerHint   = GetEulerAngles();
    } else if (obj.HasKey("rotation")) {
        SetEulerRotation(obj.GetVec3("rotation", eulerHint));
    }

    scale = obj.GetVec3("scale", scale);

    MarkDirty();
    UpdateMatrix();
}
