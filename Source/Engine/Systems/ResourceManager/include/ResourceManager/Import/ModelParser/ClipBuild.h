#pragma once

#include <AnimationSystem/AnimClip.h>

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <string>
#include <vector>

// Pure half of clip extraction: ticks -> seconds. Same rule as SkeletonBuild.h --
// no assimp types, so the conversion is testable on hand-built input.
namespace nous::engine::resource_manager
{
    // One channel straight off aiNodeAnim, still in TICKS and still double.
    struct RawChannel
    {
        std::string boneName;

        std::vector<double>    posTimes;    std::vector<glm::vec3> posValues;
        std::vector<double>    rotTimes;    std::vector<glm::quat> rotValues;
        std::vector<double>    scaleTimes;  std::vector<glm::vec3> scaleValues;
    };

    struct RawClip
    {
        std::string             name;
        double                  durationTicks   = 0.0;
        double                  ticksPerSecond  = 0.0;
        std::vector<RawChannel> channels;
    };

    // FBX exporters very often report mTicksPerSecond as 0, and assimp passes that
    // through unchanged. Dividing by it is a crash or an infinity; treating 0 as
    // "1 tick per second" makes a 24 fps clip play 24x too slow, which reads as an
    // animation bug rather than an import bug.
    //
    // 25.0 is assimp's own documented default for the field. Returns it for 0,
    // negatives and non-finite values alike.
    [[nodiscard]] double ResolveTicksPerSecond(double reported);

    // Converts every track to seconds and drops the tick rate on the floor.
    //
    // The rate deliberately does NOT survive into AnimClipData: if it did, every
    // downstream site would have to remember to divide, and the one that forgets
    // plays at 25x. One conversion, here, at the boundary.
    //
    // Channels whose three tracks are inconsistent (times and values of different
    // lengths) are dropped rather than fixed -- a half-written channel would sample
    // as garbage, and the caller logs the count.
    [[nodiscard]] animation_system::AnimClipData BuildClip(const RawClip& raw,
                                                           size_t* outDroppedChannels = nullptr);
}
