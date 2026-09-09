#pragma once

#include <AnimationSystem/Transform.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace nous::engine::animation_system
{
    struct AnimationBinding;
    struct Pose;

    /**
     * @brief One frame's worth of travel taken out of a clip's root bone.
     *
     * HORIZONTAL TRANSLATION AND YAW ONLY. Y stays in the pose because vertical
     * motion in a gait is the hip bob, and hoisting it onto the GameObject makes
     * the character bounce through whatever it stands on -- there is no collision
     * to resolve against. Pitch and roll stay for the same reason.
     */
    struct RootMotionDelta
    {
        glm::vec3 translation{ 0.0f };   // XZ; y is always 0
        float     yaw = 0.0f;            // radians about +Y
    };

    /**
     * @brief The lowest-index bone the clip drives, or -1 when it drives none.
     *
     * NOT bone 0. Mixamo's bone 0 is a static RootNode at the origin with no
     * channel, so its delta is a constant zero and root motion silently does
     * nothing. Because bone order is topological, the lowest driven index is the
     * highest ancestor the animation actually controls -- Hips on a Mixamo rig, a
     * dedicated animated root node on a rig that has one.
     */
    [[nodiscard]] int ResolveRootBone(const AnimationBinding& binding);

    /**
     * @brief Travel between two root samples, correct across a loop seam.
     *
     * `wrapped` says Advance() wrapped this frame. Without it the delta at the
     * seam is one whole cycle BACKWARDS, because the root snaps from the end of
     * its travel to the start -- the character teleports back exactly as far as it
     * just walked, every loop. clipStart/clipEnd are the root's transform at t=0
     * and t=duration; they never change for a bound clip, so the caller caches
     * them at bind time and this stays pure arithmetic.
     */
    [[nodiscard]] RootMotionDelta ComputeRootDelta(const Transform& previous,
                                                   const Transform& current,
                                                   const Transform& clipStart,
                                                   const Transform& clipEnd,
                                                   bool             wrapped);

    /**
     * @brief Moves the pose's root to its bind-pose horizontal placement and yaw.
     *
     * Bind-pose rather than zero: it is the rig's own rest position, so it cannot
     * be wrong for a given skeleton and does not depend on which clip is playing.
     * Zeroing instead would shift any rig whose bind pose offsets the hips.
     *
     * A rootBone outside the pose (including -1) is a no-op -- a clip that drives
     * nothing still reaches here.
     */
    void StripRootMotion(Pose& pose, int rootBone, const Transform& bindLocal);

    /**
     * @brief Mixes two tracks' deltas by the cross-fade weight.
     *
     * Blend the DELTAS, never the blended pose's position. The blended root
     * position sweeps from one clip's root to the other's as the weight moves
     * 0->1, and that sweep is an artifact of blending two unrelated clips rather
     * than motion -- a positional delta cannot tell them apart, so it injects a
     * lurch on every transition, proportional to how far apart the two clips
     * happen to have their hips. This is a velocity blend, which is what it should
     * have been.
     */
    [[nodiscard]] RootMotionDelta BlendRootDelta(const RootMotionDelta& a,
                                                 const RootMotionDelta& b,
                                                 float                  weight);

    // Heading of the rotated forward vector, in radians about +Y. Exposed for
    // testing and reused by the strip. A bone pitched to vertical has no
    // meaningful heading and returns 0.
    [[nodiscard]] float ExtractYaw(const glm::quat& rotation);
}
