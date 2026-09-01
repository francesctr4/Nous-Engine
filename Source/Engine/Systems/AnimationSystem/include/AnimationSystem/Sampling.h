#pragma once

#include <AnimationSystem/AnimInstance.h>
#include <AnimationSystem/Pose.h>
#include <AnimationSystem/Skeleton.h>

#include <cstddef>
#include <vector>

namespace nous::engine::animation_system
{
    // Advances instance.time by dt * speed and applies the loop policy.
    //
    // Looping wraps with fmod and resets the cursor, so a wrap is O(channels)
    // rather than O(keys). Non-looping clamps to [0, duration] and stops; ask
    // IsFinished() rather than comparing time yourself, because a clip with
    // duration 0 is finished immediately and the naive comparison says otherwise.
    void Advance(AnimInstance& instance, float dt);

    [[nodiscard]] bool IsFinished(const AnimInstance& instance);

    // Samples the instance at its current time into outPose, in LOCAL space.
    //
    // outPose is resized to the skeleton's bone count and stamped with skeletonUID,
    // so a caller may pass an empty Pose -- though CAnimator preallocates in Bind(),
    // since resizing three poses per character per frame is pointless churn.
    //
    // The UID is a parameter rather than a field on SkeletonData because the pure
    // library has no notion of resources; the UID only exists so Blend() can refuse
    // to mix poses from two different rigs, and it is the caller (which owns the
    // ResourceSkeleton) that knows it.
    //
    // EVERY bone is written. Bones no channel drives get skeleton.bindLocals[i],
    // which is why bindLocals exists: leaving them at default-constructed identity
    // collapses the un-animated half of the rig onto the origin, and the failure
    // reads as a skinning bug rather than a sampling one.
    //
    // A track with no keys leaves that component at the bind value; a track with
    // one key pins it. Times outside [first, last] clamp to the end keys -- they do
    // NOT wrap, because wrapping is Advance()'s job and doing it in both places
    // double-counts at the seam.
    //
    // Non-const instance: the per-channel cursor advances here. That is the whole
    // point of the cursor, and it is why sampling one instance from two threads is
    // not safe (sampling different instances in parallel is, which is what the
    // spec's PreUpdate job-system note relies on).
    void Sample(AnimInstance& instance, const SkeletonData& skeleton, uint32_t skeletonUID,
                Pose& outPose);

    // ---- exposed for testing --------------------------------------------------
    //
    // Finds the key index i such that times[i] <= t < times[i+1], starting from
    // `cursor` and walking forward; restarts from 0 if t is behind the cursor, so a
    // backwards seek is correct even without a ResetCursor(). Writes the updated
    // index back through `cursor` and returns the interpolation factor in [0, 1].
    //
    // Empty times -> factor 0, cursor 0. Single key or t past the last key -> the
    // last index with factor 0, so callers can interpolate unconditionally.
    struct KeyLocation
    {
        size_t index  = 0;   // first key of the bracketing pair
        float  factor = 0.0f;
    };

    [[nodiscard]] KeyLocation FindKey(const std::vector<float>& times, float t, uint32_t& cursor);
}
