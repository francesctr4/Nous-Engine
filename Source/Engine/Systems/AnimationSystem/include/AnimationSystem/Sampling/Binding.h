#pragma once

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace nous::engine::animation_system
{
    struct AnimClipData;
    struct SkeletonData;

    // Resolves one clip's channels against one skeleton's bones, once, so the per-frame
    // path is pure integer indexing. Channels address bones by NAME -- that is all an FBX
    // gives you -- and doing that lookup per bone per frame per character is the obvious
    // way to make animation the profile's hot spot.
    struct AnimationBinding
    {
        uint32_t animation = 0;
        uint32_t skeleton  = 0;

        // Parallel to the clip's channels. -1 where a channel names a bone this skeleton
        // does not have: a normal, non-fatal case, since clips get retargeted and exporters
        // emit channels for helper nodes that never became bones.
        std::vector<int> channelToBone;

        // The bone root motion is read from: the lowest index in channelToBone, or -1 when
        // this clip drives nothing in this skeleton. Resolved here because it is a property
        // of exactly this (clip, skeleton) pair, and this struct is rebuilt whenever either
        // changes -- so it cannot go stale.
        int rootBone = -1;
    };

    // Fills channelToBone by name lookup. Cheap, but not free: call once and cache.
    [[nodiscard]] AnimationBinding CreateBinding(const AnimClipData& clip, uint32_t clipUID,
                                                 const SkeletonData& skeleton, uint32_t skeletonUID);

    // Keyed on (clip UID, skeleton UID). NOTHING OWNS ONE YET -- CAnimator holds its
    // bindings inline. It belongs neither in CAnimator nor in the resource: twenty
    // skeletons playing one walk cycle share a single binding, and the resource layer has
    // no business knowing which skeletons it might be paired with.
    //
    // Returned pointers are stable across further Get() calls (node-based map) but NOT
    // across Clear(). An AnimInstance holds one, so clear only when nothing is playing.
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
        // Both halves are 32-bit UIDs, so packing them is lossless and saves hashing a pair.
        static uint64_t MakeKey(uint32_t clipUID, uint32_t skeletonUID)
        {
            return (static_cast<uint64_t>(clipUID) << 32) | skeletonUID;
        }

        std::unordered_map<uint64_t, AnimationBinding> m_entries;
    };
}
