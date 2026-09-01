#pragma once

#include <AnimationSystem/Pose.h>

namespace nous::engine::animation_system
{
    // Per-bone Interpolate() from a to b. weight 0 yields a, weight 1 yields b,
    // both bit-exact (Interpolate early-outs) -- a cross-fade that has finished
    // must return the target pose untouched, not something a slerp rounded near it.
    //
    // Returns false and leaves `out` alone when the poses are incompatible:
    // different skeleton UIDs, or different bone counts. Bone index 7 means a
    // different joint on a different rig, so blending across skeletons produces
    // confident garbage -- the spec asks for an assert here, and this library
    // cannot assert without taking a dependency on Logger/Asserts and forfeiting
    // the zero-dependency property that makes it testable. A checked bool that
    // callers must consume ([[nodiscard]]) buys the same protection; ModuleAnimation
    // is where the engine-side NOUS_ASSERT on the return value belongs.
    //
    // `out` may alias neither a nor b.
    [[nodiscard]] bool Blend(const Pose& a, const Pose& b, float weight, Pose& out);

    [[nodiscard]] bool ArePosesCompatible(const Pose& a, const Pose& b);
}
