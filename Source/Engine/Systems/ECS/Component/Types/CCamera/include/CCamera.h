#pragma once

#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/EngineExport.h"
#include <glm/glm.hpp>

/**
 * @brief Camera component for GameObjects.
 *
 * Attaching this component to a GameObject makes it a camera in the scene.
 * When `isMainCamera` is true, this camera drives the Game View rendering
 * by syncing its parameters each frame to `ModuleScene::gameCamera`.
 *
 * Position and orientation are taken from the sibling CTransform component.
 */
class CCamera : public Component {
public:
    COMPONENT_TYPE(CCamera)

    float fov         = 60.0f;      // Vertical FOV in degrees
    float nearPlane   = 0.1f;
    float farPlane    = 1000.0f;
    float aspectRatio = 16.0f / 9.0f;
    bool  isMainCamera = true;

    // Return view matrix derived from sibling CTransform.
    NOUS_ENGINE_API glm::mat4 GetViewMatrix() const;

    // Return projection matrix from FOV / aspect / near / far.
    NOUS_ENGINE_API glm::mat4 GetProjectionMatrix() const;

    // Lifecycle
    NOUS_ENGINE_API void OnUpdate(float deltaTime) override;

    // Serialization
    NOUS_ENGINE_API JsonObject Serialize() const override;
    NOUS_ENGINE_API void       Deserialize(const JsonObject& obj) override;
};
