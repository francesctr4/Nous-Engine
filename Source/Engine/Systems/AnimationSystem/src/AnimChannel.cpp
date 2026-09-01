#include <AnimationSystem/AnimChannel.h>

namespace nous::engine::animation_system
{
    namespace
    {
        bool IsNonDecreasing(const std::vector<float>& times)
        {
            for (size_t i = 1; i < times.size(); ++i)
            {
                if (times[i] < times[i - 1]) return false;
            }
            return true;
        }
    }

    bool AnimChannel::IsConsistent() const
    {
        if (posTimes.size()   != posValues.size())   return false;
        if (rotTimes.size()   != rotValues.size())   return false;
        if (scaleTimes.size() != scaleValues.size()) return false;

        // Sampling walks the cursor forward on the assumption that times ascend.
        // Out-of-order keys would not crash it -- they would make it interpolate
        // backwards over a random pair, which is far harder to spot.
        return IsNonDecreasing(posTimes)
            && IsNonDecreasing(rotTimes)
            && IsNonDecreasing(scaleTimes);
    }
}
