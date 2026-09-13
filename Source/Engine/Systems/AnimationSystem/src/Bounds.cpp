#include <AnimationSystem/Bounds.h>

#include <algorithm>
#include <limits>

namespace nous::engine::animation_system
{
    bool ComputeSkinnedBounds(std::span<const glm::vec3> boneMin,
                              std::span<const glm::vec3> boneMax,
                              std::span<const glm::mat4> palette,
                              glm::vec3& outMin,
                              glm::vec3& outMax)
    {
        if (palette.empty() || boneMin.empty() || boneMin.size() != boneMax.size())
            return false;

        // A mesh may reference fewer bones than its skeleton has, so the shorter of
        // the two is the range that has both a box and a matrix.
        const size_t count = std::min(palette.size(), boneMin.size());

        glm::vec3 lo( std::numeric_limits<float>::max());
        glm::vec3 hi(-std::numeric_limits<float>::max());

        bool any = false;

        for (size_t b = 0; b < count; ++b)
        {
            const glm::vec3& bMin = boneMin[b];
            const glm::vec3& bMax = boneMax[b];

            // Inverted box == this bone influences no vertex. Transforming it would
            // fold FLT_MAX corners into the union and swallow the whole scene.
            if (bMin.x > bMax.x || bMin.y > bMax.y || bMin.z > bMax.z)
                continue;

            const glm::vec3 corners[8] = {
                { bMin.x, bMin.y, bMin.z }, { bMax.x, bMin.y, bMin.z },
                { bMin.x, bMax.y, bMin.z }, { bMax.x, bMax.y, bMin.z },
                { bMin.x, bMin.y, bMax.z }, { bMax.x, bMin.y, bMax.z },
                { bMin.x, bMax.y, bMax.z }, { bMax.x, bMax.y, bMax.z },
            };

            const glm::mat4& m = palette[b];

            for (const glm::vec3& c : corners)
            {
                const glm::vec3 p = glm::vec3(m * glm::vec4(c, 1.0f));
                lo = glm::min(lo, p);
                hi = glm::max(hi, p);
            }

            any = true;
        }

        if (!any)
            return false;

        outMin = lo;
        outMax = hi;
        return true;
    }
}
