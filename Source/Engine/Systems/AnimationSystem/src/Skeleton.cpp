#include <AnimationSystem/Skeleton.h>

namespace nous::engine::animation_system
{
    int SkeletonData::FindBone(std::string_view boneName) const
    {
        // Heterogeneous lookup would avoid the temporary std::string, but it needs a
        // transparent hash on the map and this runs once per bone in CAnimator::Bind,
        // never per frame. Not worth the extra machinery yet.
        const auto it = lookup.find(std::string(boneName));
        return it == lookup.end() ? -1 : static_cast<int>(it->second);
    }

    void SkeletonData::RebuildLookup()
    {
        lookup.clear();
        lookup.reserve(names.size());

        for (uint32_t i = 0; i < names.size(); ++i)
        {
            // First occurrence wins. Duplicate bone names are an authoring error;
            // resolving to the first keeps behaviour deterministic instead of
            // depending on iteration order.
            lookup.emplace(names[i], i);
        }
    }

    bool SkeletonData::IsConsistent() const
    {
        const size_t n = names.size();
        return parents.size() == n && offsets.size() == n && bindLocals.size() == n;
    }

    bool SkeletonData::IsTopologicallySorted() const
    {
        if (parents.size() != names.size()) return false;

        for (size_t i = 0; i < parents.size(); ++i)
        {
            const int parent = parents[i];

            if (parent < 0) continue;                                  // root
            if (static_cast<size_t>(parent) >= i) return false;         // forward ref
        }

        return true;
    }
}
