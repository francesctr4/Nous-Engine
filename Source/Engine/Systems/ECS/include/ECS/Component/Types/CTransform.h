#pragma once

#include <ECS/Component/Component.h>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include "Engine/EngineExport.h"

class CTransform : public Component {
public:
    COMPONENT_TYPE(CTransform)

    glm::vec3 position {0.0f, 0.0f, 0.0f};
    glm::quat orientation {1.0f, 0.0f, 0.0f, 0.0f}; // Identity quaternion (w,x,y,z)
    glm::vec3 scale    {1.0f, 1.0f, 1.0f};

    // Euler angle hint for Inspector display (degrees). Kept in sync but NOT the source of truth.
    glm::vec3 eulerHint {0.0f, 0.0f, 0.0f};

    // Cached world matrix (updated by UpdateMatrix)
    glm::mat4 worldMatrix {1.0f};

    // Dirty flags for skipping redundant UpdateMatrix/AABB work on static objects.
    // m_localDirty: local transform fields changed — set by MarkDirty(), cleared by UpdateWorldMatrices.
    // m_worldDirty: worldMatrix was recomputed this frame — read by AABB cache, cleared after AABB update.
    bool m_localDirty = true;
    bool m_worldDirty = true;

    void MarkDirty() { m_localDirty = true; }

    // Helper methods
    NOUS_ENGINE_API glm::mat4 GetLocalMatrix() const;

    void SetPosition(const glm::vec3& newPosition) { position = newPosition; MarkDirty(); }
    NOUS_ENGINE_API void SetEulerRotation(const glm::vec3& eulerDegrees);
    void SetOrientation(const glm::quat& quat) { orientation = quat; eulerHint = GetEulerAngles(); MarkDirty(); }
    void SetScale(const glm::vec3& newScale) { scale = newScale; MarkDirty(); }

    void Translate(const glm::vec3& translation) { position += translation; MarkDirty(); }
    NOUS_ENGINE_API void Rotate(const glm::quat& deltaRotation);

    // Get Euler angles in degrees from the current orientation
    NOUS_ENGINE_API glm::vec3 GetEulerAngles() const;

    // Forward vector based on orientation
    NOUS_ENGINE_API glm::vec3 GetForward() const;

    // Right vector based on orientation
    NOUS_ENGINE_API glm::vec3 GetRight() const;

    // Up vector based on orientation
    NOUS_ENGINE_API glm::vec3 GetUp() const;

    NOUS_ENGINE_API void UpdateMatrix();

    // ---------- JSON Serialization ----------
    NOUS_ENGINE_API JsonObject Serialize() const override;

    NOUS_ENGINE_API void Deserialize(const JsonObject& obj) override;
};
