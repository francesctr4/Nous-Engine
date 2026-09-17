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
     * Horizontal translation and yaw only. Y stays in the pose because vertical motion in
     * a gait is the hip bob, and there is no collision to resolve a hoisted character
     * against; pitch and roll stay for the same reason.
     */
    struct RootMotionDelta
    {
        glm::vec3 translation{ 0.0f };   // XZ; y is always 0
        float     yaw = 0.0f;            // radians about +Y
    };

    /**
     * @brief The lowest-index bone the clip drives, or -1 when it drives none.
     *
     * NOT bone 0: Mixamo's is a static RootNode at the origin with no channel, so its
     * delta is a constant zero and root motion silently does nothing. Bone order is
     * topological, so the lowest driven index is the highest ancestor the clip controls.
     */
    [[nodiscard]] int ResolveRootBone(const AnimationBinding& binding);

    /**
     * @brief Travel between two root samples, correct across a loop seam.
     *
     * The translation comes back in the ROOT'S OWN FRAME at the previous sample, not the
     * clip's fixed frame. The consumer rotates it by the GameObject's orientation, which
     * already carries every yaw this has handed back, so an un-de-rotated delta applies
     * the clip's own turning twice -- a clip that turns 180 degrees then walks forward
     * drives the transform exactly backwards while the pose walks forwards.
     *
     * `wrapped` says Advance() wrapped this frame. Without it the delta at the seam is one
     * whole cycle BACKWARDS. clipStart/clipEnd are the root at t=0 and t=duration; they
     * never change for a bound clip, so the caller caches them and this stays arithmetic.
     *
     * `reversed` matters only on a wrapped frame, where the seam is crossed the other way:
     * applying the forward split to a backward wrap does not flip a sign, it measures
     * almost the whole clip the wrong way twice (~+2x its travel).
     */
    [[nodiscard]] RootMotionDelta ComputeRootDelta(const Transform& previous,
                                                   const Transform& current,
                                                   const Transform& clipStart,
                                                   const Transform& clipEnd,
                                                   bool             wrapped,
                                                   bool             reversed = false);

    /**
     * @brief Moves the pose's root to its bind-pose horizontal placement and yaw.
     *
     * Bind-pose rather than zero: it is the rig's own rest position, so it cannot be wrong
     * for a given skeleton. A rootBone outside the pose (including -1) is a no-op.
     *
     * `stripYaw` separates the two consumers. A mode that puts the delta ON the GameObject
     * must take the yaw out of the pose or the turn is applied twice; a mode that DISCARDS
     * it must leave it in, since the yaw is going nowhere and removing it deletes
     * animation rather than travel. Mixamo's In Place export draws the line the same way.
     */
    void StripRootMotion(Pose& pose, int rootBone, const Transform& bindLocal, bool stripYaw);

    /**
     * @brief Mixes two tracks' deltas by the cross-fade weight.
     *
     * Blend the DELTAS, never the blended pose's position: that position sweeps from one
     * clip's root to the other's as the weight moves 0->1, and a positional delta cannot
     * tell that artifact from motion -- so it would lurch on every transition,
     * proportionally to how far apart the two clips have their hips.
     */
    [[nodiscard]] RootMotionDelta BlendRootDelta(const RootMotionDelta& a,
                                                 const RootMotionDelta& b,
                                                 float                  weight);

    // Heading of the rotated forward vector, in radians about +Y. A bone pitched to
    // vertical has no meaningful heading and returns 0.
    [[nodiscard]] float ExtractYaw(const glm::quat& rotation);
}
