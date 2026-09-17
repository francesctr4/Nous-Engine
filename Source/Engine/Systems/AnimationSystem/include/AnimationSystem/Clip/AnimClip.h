#pragma once

#include <AnimationSystem/Clip/AnimChannel.h>

#include <string>
#include <vector>

namespace nous::engine::animation_system
{
    // A clip as plain data: the sampler's entire view of "an animation".
    //
    // ResourceAnimation holds one BY VALUE, so the sampler never learns that resources
    // exist -- the same split SkeletonData uses, and what keeps ResourceManager out of this
    // library.
    struct AnimClipData
    {
        std::string              name;
        float                    duration = 0.0f;   // SECONDS, see AnimChannel.h
        std::vector<AnimChannel> channels;

        [[nodiscard]] size_t ChannelCount() const { return channels.size(); }
    };
}
