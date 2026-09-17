#pragma once

#include <AnimationSystem/Transform.h>

#include <cstdint>
#include <vector>

namespace nous::engine::animation_system
{
    // One frame of animation: a LOCAL-space transform per bone. Local, not global --
    // blending two global poses is meaningless, since the parent's contribution is baked
    // into every child, and the hierarchy is walked once at the end anyway.
    //
    // `skeleton` is the UID of the SkeletonData these indices refer to: bone index 3 means
    // nothing on its own, so every operation taking two poses checks the UIDs match first.
    struct Pose
    {
        uint32_t               skeleton = 0;
        std::vector<Transform> bones;

        [[nodiscard]] size_t BoneCount() const { return bones.size(); }
    };
}
