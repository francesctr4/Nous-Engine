#ifndef NOUS_ENGINE_COMPONENTTRANSFORM_H
#define NOUS_ENGINE_COMPONENTTRANSFORM_H

#include "ECS/Component.h"
#include "Includes/glmath.h"

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

};

#endif //NOUS_ENGINE_COMPONENTTRANSFORM_H
