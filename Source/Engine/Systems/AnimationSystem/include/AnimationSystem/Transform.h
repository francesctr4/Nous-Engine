#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// This library is a pure data + free-function layer: glm is its only dependency, which is
// what lets its tests link this archive and gtest and nothing else.
//
// `Transform` MUST stay namespaced -- the ECS already owns a global `CTransform`, and a
// bare global `Transform` in a public header would be an ambient name for the whole tree.
namespace nous::engine::animation_system
{
    // A TRS triple, the value the whole pipeline moves around. Deliberately NOT a matrix:
    // interpolating matrices componentwise shears, and decomposing one per bone per frame
    // to fix that is pure waste.
    struct Transform
    {
        glm::vec3 position{ 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };  // (w, x, y, z) identity
        glm::vec3 scale{ 1.0f };

        // T * R * S, matching CTransform::GetLocalMatrix so a bone and a GameObject
        // transform compose the same way.
        [[nodiscard]] glm::mat4 ToMatrix() const;
    };

    // Component-wise lerp on position/scale, slerp on rotation.
    //
    // glm::slerp negates the target when dot(a, b) < 0, so the interpolation takes the
    // short arc. That is load-bearing and pinned by a test rather than assumed: without it
    // a 181-degree shoulder rotation sweeps the long way and the arm passes through the
    // torso.
    //
    // t is clamped to [0, 1], and t == 0 / t == 1 return their source bit-exact (an
    // early-out, not "close enough") -- a finished cross-fade must hand back the target
    // pose untouched, and glm::slerp at t == 1 returns a normalized b rather than b.
    [[nodiscard]] Transform Interpolate(const Transform& a, const Transform& b, float t);
}
