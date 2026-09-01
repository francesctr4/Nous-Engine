#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace nous::engine::animation_system
{
    struct AnimClipData;
    struct SkeletonData;

    // Resolves one clip's channels against one skeleton's bones, once, so that the
    // per-frame path is pure integer indexing. Channels address bones by NAME
    // (that is all an FBX gives you); doing that string lookup per bone per frame
    // per character is the obvious way to make animation the profile's hot spot.
    struct AnimationBinding
    {
        uint32_t animation = 0;
        uint32_t skeleton  = 0;

        // Parallel to the clip's channels. -1 where a channel names a bone this
        // skeleton does not have -- a normal, non-fatal case: clips get retargeted,
        // and exporters emit channels for helper nodes that never became bones.
        std::vector<int> channelToBone;
    };

    // Fills channelToBone by name lookup. Cheap, but not free -- call it once and
    // cache the result, which is what BindingCache below is for.
    [[nodiscard]] AnimationBinding CreateBinding(const AnimClipData& clip, uint32_t clipUID,
                                                 const SkeletonData& skeleton, uint32_t skeletonUID);

    // Keyed on (clip UID, skeleton UID). ModuleAnimation owns one; it lives neither
    // in CAnimator nor in the resource, for two reasons that pull the same way:
    // twenty skeletons playing one walk cycle share a single binding, and the
    // resource layer has no business knowing which skeletons it might be paired
    // with. Invalidate on resource reload, which starts mattering once FBX
    // hot-reload exists.
    //
    // Returned pointers are stable across further Get() calls (node-based map), but
    // NOT across Clear(). AnimInstance holds one, so clear only when nothing is
    // playing -- on scene teardown or resource reload.
    class BindingCache
    {
    public:
        [[nodiscard]] const AnimationBinding* Get(const AnimClipData& clip, uint32_t clipUID,
                                                  const SkeletonData& skeleton, uint32_t skeletonUID);

        void InvalidateAnimation(uint32_t clipUID);
        void InvalidateSkeleton(uint32_t skeletonUID);
        void Clear();

        [[nodiscard]] size_t Size() const { return m_entries.size(); }

    private:
        // (clip, skeleton) packed into one 64-bit key: both halves are 32-bit UIDs,
        // so this is lossless and saves hashing a pair.
        static uint64_t MakeKey(uint32_t clipUID, uint32_t skeletonUID)
        {
            return (static_cast<uint64_t>(clipUID) << 32) | skeletonUID;
        }

        std::unordered_map<uint64_t, AnimationBinding> m_entries;
    };
}
