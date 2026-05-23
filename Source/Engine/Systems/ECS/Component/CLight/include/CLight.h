#pragma once

#include "Engine/Systems/ECS/Component/Component.h"
#include "Engine/EngineExport.h"
#include "Engine/Renderer/RendererTypes.h"

#include <glm/glm.hpp>

enum class LightType : uint8_t { Directional, Point, Spot };

/**
 * @brief Light component. Attach to a GameObject to make it a light source.
 *
 * Directional light: direction is derived from the sibling CTransform forward vector
 *   (orientation * vec3(0, -1, 0)) at gather time — rotate the GameObject to aim the light.
 * Point light: position is taken from the sibling CTransform::position at gather time.
 *   range (stored in PointLight::position.w) controls how far the light reaches.
 */
class CLight : public Component
{
public:
    COMPONENT_TYPE(CLight)

    LightType  type      = LightType::Directional;
    glm::vec3  color     = glm::vec3(1.0f);
    float      intensity = 1.0f;
    float      range     = 10.0f;   // Point and Spot only; ignored for directional
    float      innerAngle = 15.0f;  // Spot only, degrees — full-brightness cone half-angle
    float      outerAngle = 25.0f;  // Spot only, degrees — falloff-edge cone half-angle

    // Lifecycle — no per-frame work needed; lights are gathered in BuildRenderPacket.
    NOUS_ENGINE_API void OnUpdate(float deltaTime) override;

    // Serialization
    NOUS_ENGINE_API JsonObject Serialize()                   const override;
    NOUS_ENGINE_API void       Deserialize(const JsonObject& obj)  override;
};
