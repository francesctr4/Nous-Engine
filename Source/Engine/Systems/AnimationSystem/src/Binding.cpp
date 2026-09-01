#include <AnimationSystem/Binding.h>

#include <AnimationSystem/AnimClip.h>
#include <AnimationSystem/Skeleton.h>

#include <unordered_map>   // std::erase_if(unordered_map) overload

namespace nous::engine::animation_system
{
    AnimationBinding CreateBinding(const AnimClipData& clip, uint32_t clipUID,
                                   const SkeletonData& skeleton, uint32_t skeletonUID)
    {
        AnimationBinding binding;
        binding.animation = clipUID;
        binding.skeleton  = skeletonUID;

        binding.channelToBone.resize(clip.channels.size());

        for (size_t i = 0; i < clip.channels.size(); ++i)
        {
            // -1 for an unmatched channel, and Sample() skips those. Not an error:
            // exporters emit channels for helper nodes that never became bones, and
            // a clip authored on a fuller rig than the one playing it is normal.
            binding.channelToBone[i] = skeleton.FindBone(clip.channels[i].boneName);
        }

        return binding;
    }

    const AnimationBinding* BindingCache::Get(const AnimClipData& clip, uint32_t clipUID,
                                              const SkeletonData& skeleton, uint32_t skeletonUID)
    {
        const uint64_t key = MakeKey(clipUID, skeletonUID);

        if (const auto it = m_entries.find(key); it != m_entries.end())
        {
            return &it->second;
        }

        // unordered_map nodes are stable, so pointers handed out earlier survive
        // this insert. AnimInstance holds one of those pointers for as long as it
        // plays, which is why that stability matters and why Clear() is the one
        // operation that must not run while anything is playing.
        const auto [inserted, ok] =
            m_entries.emplace(key, CreateBinding(clip, clipUID, skeleton, skeletonUID));

        return &inserted->second;
    }

    void BindingCache::InvalidateAnimation(uint32_t clipUID)
    {
        std::erase_if(m_entries, [clipUID](const auto& entry)
        {
            return static_cast<uint32_t>(entry.first >> 32) == clipUID;
        });
    }

    void BindingCache::InvalidateSkeleton(uint32_t skeletonUID)
    {
        std::erase_if(m_entries, [skeletonUID](const auto& entry)
        {
            return static_cast<uint32_t>(entry.first & 0xFFFFFFFFull) == skeletonUID;
        });
    }

    void BindingCache::Clear()
    {
        m_entries.clear();
    }
}
