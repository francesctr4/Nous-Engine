#pragma once

#include <AnimationSystem/Transform.h>

#include <cstdint>
#include <vector>

namespace nous::engine::animation_system
{
    // One frame of animation: a LOCAL-space transform per bone, indexed by bone
    // index. Local, not global -- blending two global poses is meaningless (the
    // parent's contribution is baked into every child), and the hierarchy has to be
    // walked once at the end anyway to build the palette.
    //
    // `skeleton` is the UID of the SkeletonData these indices refer to. Bone index
    // 3 means nothing on its own, so every operation taking two poses checks the
    // UIDs match first. Plain uint32_t because that is what the resource system
    // uses for UIDs; there is no engine-wide UID alias to reach for, and reaching
    // into ResourceManager for one would cost this library its independence.
    struct Pose
    {
        uint32_t               skeleton = 0;
        std::vector<Transform> bones;

        [[nodiscard]] size_t BoneCount() const { return bones.size(); }
    };
}
