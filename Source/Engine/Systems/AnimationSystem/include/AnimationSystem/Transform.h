#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

// The animation library is a pure data + free-function layer: no ECS, no
// Modules, no ResourceManager, no Logger, no MemoryManager. glm is its only
// dependency. That is what lets the t_AnimationSystem_* tests link this archive
// and gtest and nothing else -- the same policy as AudioSystem's GainDsp and
// AudioBusMath.
//
// Everything lives in nous::engine::animation_system. `Transform` in particular
// MUST be namespaced: the ECS already owns a global `CTransform`, and a bare
// global `Transform` in a public header would be an ambient name for the tree.
namespace nous::engine::animation_system
{
    // A TRS triple -- the value the whole pipeline moves around: sampling produces
    // these, blending mixes them, palette construction consumes them. Deliberately
    // NOT a matrix: interpolating matrices componentwise is wrong (it shears), and
    // decomposing one per bone per frame to fix that is pure waste.
    struct Transform
    {
        glm::vec3 position{ 0.0f };
        glm::quat rotation{ 1.0f, 0.0f, 0.0f, 0.0f };  // (w, x, y, z) identity
        glm::vec3 scale{ 1.0f };

        // T * R * S, matching CTransform::GetLocalMatrix so a bone transform and a
        // GameObject transform compose the same way once CAnimator writes poses
        // into the scene hierarchy.
        [[nodiscard]] glm::mat4 ToMatrix() const;
    };

    // Component-wise lerp on position/scale, slerp on rotation.
    //
    // glm::slerp already negates the target quaternion when dot(a, b) < 0, so the
    // interpolation takes the short arc. That behaviour is load-bearing and is
    // pinned by a test rather than assumed: without it a 181-degree shoulder
    // rotation sweeps the long way round and the arm passes through the torso.
    //
    // t is clamped to [0, 1]. t == 0 returns `a` and t == 1 returns `b` bit-exact
    // (early-out, not "close enough") -- callers blend-out to exactly 0 or 1 and
    // expect the source pose back unperturbed.
    [[nodiscard]] Transform Interpolate(const Transform& a, const Transform& b, float t);
}
