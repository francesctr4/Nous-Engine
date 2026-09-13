#pragma once

#include <AnimationSystem/AnimChannel.h>

#include <string>
#include <vector>

namespace nous::engine::animation_system
{
    // A clip as plain data: the sampler's entire view of "an animation".
    //
    // The spec had AnimInstance point at `ResourceAnimation*` directly, which would
    // have dragged ResourceManager into this library and cost it the independence
    // that makes it testable without an Application. Same fix as SkeletonData:
    // ResourceAnimation (step 6) holds an AnimClipData BY VALUE and the sampler
    // never learns that resources exist. Nothing above loses anything -- the
    // resource still owns the storage and the UID.
    struct AnimClipData
    {
        std::string              name;
        float                    duration = 0.0f;   // SECONDS, see AnimChannel.h
        std::vector<AnimChannel> channels;

        [[nodiscard]] size_t ChannelCount() const { return channels.size(); }
    };
}
