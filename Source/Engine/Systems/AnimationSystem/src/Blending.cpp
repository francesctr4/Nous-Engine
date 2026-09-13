#include <AnimationSystem/Blending.h>

#include <AnimationSystem/Transform.h>

namespace nous::engine::animation_system
{
    bool ArePosesCompatible(const Pose& a, const Pose& b)
    {
        return a.skeleton == b.skeleton && a.bones.size() == b.bones.size();
    }

    bool Blend(const Pose& a, const Pose& b, float weight, Pose& out)
    {
        if (!ArePosesCompatible(a, b)) return false;

        out.skeleton = a.skeleton;
        out.bones.resize(a.bones.size());

        for (size_t i = 0; i < a.bones.size(); ++i)
        {
            // Interpolate early-outs at 0 and 1, so the ends are bit-exact copies
            // rather than slerp output that rounds close to them.
            out.bones[i] = Interpolate(a.bones[i], b.bones[i], weight);
        }

        return true;
    }
}
