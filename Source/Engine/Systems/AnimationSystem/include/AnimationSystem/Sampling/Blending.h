#pragma once

#include <AnimationSystem/Pose.h>

namespace nous::engine::animation_system
{
    // Per-bone Interpolate() from a to b. weight 0 yields a and weight 1 yields b, both
    // bit-exact, so a finished cross-fade returns the target pose untouched rather than
    // something a slerp rounded near it.
    //
    // Returns false and leaves `out` alone when the poses are incompatible: different
    // skeleton UIDs or bone counts. Bone index 7 is a different joint on a different rig,
    // so blending across skeletons produces confident garbage. A checked [[nodiscard]] bool
    // rather than an assert, which would cost this library its zero-dependency property;
    // the engine-side caller is where an assert on the result belongs.
    //
    // `out` may alias neither a nor b.
    [[nodiscard]] bool Blend(const Pose& a, const Pose& b, float weight, Pose& out);

    [[nodiscard]] bool ArePosesCompatible(const Pose& a, const Pose& b);
}
