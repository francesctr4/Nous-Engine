#ifndef NOUS_ENGINE_COMPONENTTRANSFORM_H
#define NOUS_ENGINE_COMPONENTTRANSFORM_H

#include "Engine/Systems/ECS/Component.h"
#include "glm/glm.hpp"
#include "Engine/EngineExport.h"

typedef struct json_object_t JSON_Object;
typedef struct json_value_t  JSON_Value;

class CTransform : public Component {
public:
    COMPONENT_TYPE(CTransform)

    glm::vec3 position {0.0f, 0.0f, 0.0f};
    glm::vec3 rotation {0.0f, 0.0f, 0.0f}; // Euler angles in degrees
    glm::vec3 scale    {1.0f, 1.0f, 1.0f};

    // Cached world matrix (updated by a system)
    glm::mat4 worldMatrix {1.0f};

    // Helper methods
    glm::mat4 GetLocalMatrix() const;

    void SetPosition(const glm::vec3& newPosition) { position = newPosition; }
    void SetRotation(const glm::vec3& newRotation) { rotation = newRotation; }
    void SetScale(const glm::vec3& newScale) { scale = newScale; }

    void Translate(const glm::vec3& translation) { position += translation; }
    void Rotate(const glm::vec3& eulerAngles) { rotation += eulerAngles; }

    // Forward vector based on rotation
    glm::vec3 GetForward() const;

    // Right vector based on rotation
    glm::vec3 GetRight() const {
        return glm::normalize(glm::cross(GetForward(), glm::vec3(0.0f, 1.0f, 0.0f)));
    }

    // Up vector based on rotation
    glm::vec3 GetUp() const;

    NOUS_ENGINE_API void UpdateMatrix();

    // ---------- JSON Serialization ----------
    JSON_Value* Serialize() const override;

    void Deserialize(JSON_Object* obj) override;
};

#endif //NOUS_ENGINE_COMPONENTTRANSFORM_H
