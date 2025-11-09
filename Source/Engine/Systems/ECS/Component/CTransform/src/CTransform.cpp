#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

// Parson
#include <parson.h>

glm::mat4 CTransform::GetLocalMatrix() const {
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);

    // Convert Euler angles to quaternion and then to matrix
    glm::quat quatRotation = glm::quat(glm::radians(rotation));
    transform *= glm::mat4_cast(quatRotation);

    transform = glm::scale(transform, scale);
    return transform;
}

glm::vec3 CTransform::GetForward() const {
    float yaw = glm::radians(rotation.y);
    float pitch = glm::radians(rotation.x);
    return glm::vec3(
            sin(yaw) * cos(pitch),
            -sin(pitch),
            cos(yaw) * cos(pitch)
    );
}

glm::vec3 CTransform::GetUp() const {
    return glm::normalize(glm::cross(GetRight(), GetForward()));
}

void CTransform::UpdateMatrix() {
    glm::quat qPitch = glm::angleAxis(glm::radians(rotation.x), glm::vec3(1,0,0));
    glm::quat qYaw   = glm::angleAxis(glm::radians(rotation.y), glm::vec3(0,1,0));
    glm::quat qRoll  = glm::angleAxis(glm::radians(rotation.z), glm::vec3(0,0,1));
    glm::quat orientation = qYaw * qPitch * qRoll;
    glm::mat4 rotationMatrix = glm::toMat4(orientation);
    worldMatrix = glm::translate(glm::mat4(1.0f), position) *
                  rotationMatrix *
                  glm::scale(glm::mat4(1.0f), scale);
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

    // Rotation
    JSON_Value* rotVal = json_value_init_array();
    JSON_Array* rotArr = json_value_get_array(rotVal);
    json_array_append_number(rotArr, rotation.x);
    json_array_append_number(rotArr, rotation.y);
    json_array_append_number(rotArr, rotation.z);
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

    // Rotation
    JSON_Array* rot = json_object_get_array(obj, "rotation");
    if (rot && json_array_get_count(rot) == 3) {
        rotation.x = static_cast<float>(json_array_get_number(rot, 0));
        rotation.y = static_cast<float>(json_array_get_number(rot, 1));
        rotation.z = static_cast<float>(json_array_get_number(rot, 2));
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
