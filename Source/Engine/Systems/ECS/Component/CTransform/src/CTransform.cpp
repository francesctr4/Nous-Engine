#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Parson
#include <parson.h>

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
}

void CTransform::Rotate(const glm::quat& deltaRotation) {
    orientation = glm::normalize(deltaRotation * orientation);
    eulerHint = GetEulerAngles();
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
    if (m_GameObject)
    {
        if (GameObject* parent = m_GameObject->GetParent())
        {
            if (CTransform* pt = parent->TryGetComponent<CTransform>())
            {
                worldMatrix = pt->worldMatrix * local;
                return;
            }
        }
    }

    worldMatrix = local;
}

JSON_Value *CTransform::Serialize() const {
    JSON_Value* objVal = json_value_init_object();
    JSON_Object* obj = json_value_get_object(objVal);

    json_object_set_string(obj, "type", GetType().c_str());

    // Position
    JSON_Value* posVal = json_value_init_array();
    JSON_Array* posArr = json_value_get_array(posVal);
    json_array_append_number(posArr, position.x);
    json_array_append_number(posArr, position.y);
    json_array_append_number(posArr, position.z);
    json_object_set_value(obj, "position", posVal);

    // Orientation (quaternion: w, x, y, z)
    JSON_Value* quatVal = json_value_init_array();
    JSON_Array* quatArr = json_value_get_array(quatVal);
    json_array_append_number(quatArr, orientation.w);
    json_array_append_number(quatArr, orientation.x);
    json_array_append_number(quatArr, orientation.y);
    json_array_append_number(quatArr, orientation.z);
    json_object_set_value(obj, "orientation", quatVal);

    // Also save Euler hint for human readability and backward compat
    JSON_Value* rotVal = json_value_init_array();
    JSON_Array* rotArr = json_value_get_array(rotVal);
    json_array_append_number(rotArr, eulerHint.x);
    json_array_append_number(rotArr, eulerHint.y);
    json_array_append_number(rotArr, eulerHint.z);
    json_object_set_value(obj, "rotation", rotVal);

    // Scale
    JSON_Value* scaleVal = json_value_init_array();
    JSON_Array* scaleArr = json_value_get_array(scaleVal);
    json_array_append_number(scaleArr, scale.x);
    json_array_append_number(scaleArr, scale.y);
    json_array_append_number(scaleArr, scale.z);
    json_object_set_value(obj, "scale", scaleVal);

    return objVal;
}

void CTransform::Deserialize(JSON_Object *obj) {
    // Position
    JSON_Array* pos = json_object_get_array(obj, "position");
    if (pos && json_array_get_count(pos) == 3) {
        position.x = static_cast<float>(json_array_get_number(pos, 0));
        position.y = static_cast<float>(json_array_get_number(pos, 1));
        position.z = static_cast<float>(json_array_get_number(pos, 2));
    }

    // Orientation (quaternion) — preferred if present
    JSON_Array* quat = json_object_get_array(obj, "orientation");
    if (quat && json_array_get_count(quat) == 4) {
        orientation.w = static_cast<float>(json_array_get_number(quat, 0));
        orientation.x = static_cast<float>(json_array_get_number(quat, 1));
        orientation.y = static_cast<float>(json_array_get_number(quat, 2));
        orientation.z = static_cast<float>(json_array_get_number(quat, 3));
        eulerHint = GetEulerAngles();
    }
    else {
        // Backward compat: load from Euler angles if no quaternion saved
        JSON_Array* rot = json_object_get_array(obj, "rotation");
        if (rot && json_array_get_count(rot) == 3) {
            glm::vec3 euler;
            euler.x = static_cast<float>(json_array_get_number(rot, 0));
            euler.y = static_cast<float>(json_array_get_number(rot, 1));
            euler.z = static_cast<float>(json_array_get_number(rot, 2));
            SetEulerRotation(euler);
        }
    }

    // Scale
    JSON_Array* scl = json_object_get_array(obj, "scale");
    if (scl && json_array_get_count(scl) == 3) {
        scale.x = static_cast<float>(json_array_get_number(scl, 0));
        scale.y = static_cast<float>(json_array_get_number(scl, 1));
        scale.z = static_cast<float>(json_array_get_number(scl, 2));
    }

    UpdateMatrix();
}
