#ifndef NOUS_ENGINE_COMPONENTTRANSFORM_H
#define NOUS_ENGINE_COMPONENTTRANSFORM_H

#include "ECS/Component.h"
#include "Includes/glmath.h"
#include "Includes/Parson.h"

class CTransform : public Component {
public:
    COMPONENT_TYPE(CTransform)

    glm::vec3 position {0.0f, 0.0f, 0.0f};
    glm::vec3 rotation {0.0f, 0.0f, 0.0f}; // Euler angles in degrees
    glm::vec3 scale    {1.0f, 1.0f, 1.0f};

    // Cached world matrix (updated by a system)
    glm::mat4 worldMatrix {1.0f};

    // Helper methods
    glm::mat4 GetLocalMatrix() const {
        glm::mat4 transform = glm::translate(glm::mat4(1.0f), position);

        // Convert Euler angles to quaternion and then to matrix
        glm::quat quatRotation = glm::quat(glm::radians(rotation));
        transform *= glm::mat4_cast(quatRotation);

        transform = glm::scale(transform, scale);
        return transform;
    }

    void SetPosition(const glm::vec3& newPosition) { position = newPosition; }
    void SetRotation(const glm::vec3& newRotation) { rotation = newRotation; }
    void SetScale(const glm::vec3& newScale) { scale = newScale; }

    void Translate(const glm::vec3& translation) { position += translation; }
    void Rotate(const glm::vec3& eulerAngles) { rotation += eulerAngles; }

    // Forward vector based on rotation
    glm::vec3 GetForward() const {
        float yaw = glm::radians(rotation.y);
        float pitch = glm::radians(rotation.x);
        return glm::vec3(
                sin(yaw) * cos(pitch),
                -sin(pitch),
                cos(yaw) * cos(pitch)
        );
    }

    // Right vector based on rotation
    glm::vec3 GetRight() const {
        return glm::normalize(glm::cross(GetForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    // Up vector based on rotation
    glm::vec3 GetUp() const {
        return glm::normalize(glm::cross(GetRight(), GetForward()));
    }

    void UpdateMatrix() {
        glm::quat qPitch = glm::angleAxis(glm::radians(rotation.x), glm::vec3(1,0,0));
        glm::quat qYaw   = glm::angleAxis(glm::radians(rotation.y), glm::vec3(0,1,0));
        glm::quat qRoll  = glm::angleAxis(glm::radians(rotation.z), glm::vec3(0,0,1));
        glm::quat orientation = qYaw * qPitch * qRoll;
        glm::mat4 rotationMatrix = glm::toMat4(orientation);
        worldMatrix = glm::translate(glm::mat4(1.0f), position) *
                      rotationMatrix *
                      glm::scale(glm::mat4(1.0f), scale);
    }

    // ---------- JSON Serialization ----------
    JSON_Value* Serialize() const override {
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

    void Deserialize(JSON_Object* obj) override
    {
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
};

#endif //NOUS_ENGINE_COMPONENTTRANSFORM_H
