#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <cstdint>

// -----------------------------------------------------------------------------
// The DECISIONS inside ModuleRenderer3D::BuildRenderPacket, separated from the
// entt iteration that applies them.
// -----------------------------------------------------------------------------
//
// BuildRenderPacket is 166 lines of registry walking wrapped around a handful of
// rules, and the rules are the part that is easy to get subtly wrong:
//
//   * which frustums are active differs between EDITOR and GAME, and in EDITOR
//     the game frustum is applied even though frustum culling is "off";
//   * a mesh MISSING from the AABB cache must be treated as VISIBLE, not culled
//     -- it has simply not been measured yet, and culling it makes objects wink
//     out on the frame they are spawned;
//   * lights point down local -Y, and a spot's angles are stored as the COSINE
//     of the half-angles in (inner, outer) order.
//
// None of that needs a registry, a camera or a device, so it lives here and is
// unit-tested directly. The iteration stays in the module.

namespace nous::engine::renderer
{
    // -------------------------------------------------------------------------
    // Which frustums to build
    // -------------------------------------------------------------------------
    //
    // EDITOR draws two passes (the scene viewport and the game preview) and culls
    // both every frame regardless of the frustumCullingEnabled toggle -- that flag
    // is a GAME-mode shipping option, not an editor one. GAME draws a single pass
    // and honours the flag.

    [[nodiscard]] constexpr bool ShouldBuildSceneFrustum(const bool isEditorMode,
                                                         const bool hasEditorCamera) noexcept
    {
        return isEditorMode && hasEditorCamera;
    }

    [[nodiscard]] constexpr bool ShouldBuildGameFrustum(const bool isEditorMode,
                                                        const bool hasGameCamera,
                                                        const bool frustumCullingEnabled) noexcept
    {
        return hasGameCamera && (isEditorMode || frustumCullingEnabled);
    }

    // -------------------------------------------------------------------------
    // Visibility, including the cache-miss rule
    // -------------------------------------------------------------------------
    //
    // `foundInCache` is false for a mesh whose world AABB has not been computed
    // yet -- typically the frame it is spawned, or any frame in GAME mode where
    // the cache is only filled when culling is enabled. Such a mesh must be drawn:
    // culling on missing data makes objects flicker into existence.
    [[nodiscard]] constexpr bool IsGeometryVisible(const bool frustumActive,
                                                   const bool foundInCache,
                                                   const bool aabbInsideFrustum) noexcept
    {
        if (!frustumActive)   return true;   // no culling for this pass
        if (!foundInCache)    return true;   // not measured yet -- never cull on absence
        return aabbInsideFrustum;
    }

    // -------------------------------------------------------------------------
    // Light conventions
    // -------------------------------------------------------------------------

    // Directional and spot lights aim down their own local -Y. This matches how
    // the light gizmos are drawn and how CLight authors rotation in the editor;
    // changing the basis vector silently re-aims every light in every scene.
    [[nodiscard]] inline glm::vec3 LightForward(const glm::quat& orientation) noexcept
    {
        return glm::normalize(orientation * glm::vec3(0.f, -1.f, 0.f));
    }

    // Spot cone angles reach the shader as COSINES of the half-angles, inner in
    // .x and outer in .y, so the fragment shader can compare against a dot
    // product without calling acos per fragment. Inner >= outer numerically,
    // because cosine decreases as the angle widens.
    [[nodiscard]] inline glm::vec4 PackSpotAngles(const float innerAngleDegrees,
                                                  const float outerAngleDegrees) noexcept
    {
        return glm::vec4(std::cos(glm::radians(innerAngleDegrees)),
                         std::cos(glm::radians(outerAngleDegrees)),
                         0.f, 0.f);
    }

    // A light's colour and its intensity share one vec4 (rgb + intensity in w);
    // position and range share another (xyz + range in w). Packing them wrongly
    // is invisible until something is lit.
    [[nodiscard]] inline glm::vec4 PackLightColor(const glm::vec3& color,
                                                  const float intensity) noexcept
    {
        return glm::vec4(color, intensity);
    }

    [[nodiscard]] inline glm::vec4 PackLightPosition(const glm::vec3& position,
                                                     const float range) noexcept
    {
        return glm::vec4(position, range);
    }

    // -------------------------------------------------------------------------
    // Light budget
    // -------------------------------------------------------------------------
    //
    // The GlobalUBO has fixed-size light arrays, so a scene may hold more lights
    // than a frame can carry. Extra lights are DROPPED with a warning rather than
    // replacing an accepted one -- first-come-first-served keeps the lit result
    // stable frame to frame instead of flickering as iteration order changes.
    [[nodiscard]] constexpr bool CanAcceptLight(const uint32_t acceptedSoFar,
                                                const uint32_t maxLights) noexcept
    {
        return acceptedSoFar < maxLights;
    }

    // Only the FIRST directional light in a scene is used; the rest are ignored.
    [[nodiscard]] constexpr bool CanAcceptDirectionalLight(const bool alreadyHaveOne) noexcept
    {
        return !alreadyHaveOne;
    }
}
