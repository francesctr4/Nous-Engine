#include <ResourceManager/Import/ModelParser/ClipBuild.h>

#include <cmath>

namespace nous::engine::resource_manager
{
    using animation_system::AnimChannel;
    using animation_system::AnimClipData;

    namespace
    {
        // assimp's documented default for aiAnimation::mTicksPerSecond.
        constexpr double c_defaultTicksPerSecond = 25.0;

        template <typename T>
        bool ConvertTrack(const std::vector<double>& inTimes, const std::vector<T>& inValues,
                          double ticksPerSecond, std::vector<float>& outTimes,
                          std::vector<T>& outValues)
        {
            if (inTimes.size() != inValues.size()) return false;

            outTimes.resize(inTimes.size());
            outValues = inValues;

            for (size_t i = 0; i < inTimes.size(); ++i)
            {
                outTimes[i] = static_cast<float>(inTimes[i] / ticksPerSecond);
            }

            return true;
        }
    }

    double ResolveTicksPerSecond(double reported)
    {
        // Covers 0 (what FBX exporters usually write), negatives, and NaN/inf in
        // one condition — every value that cannot be divided by meaningfully.
        if (!std::isfinite(reported) || reported <= 0.0) return c_defaultTicksPerSecond;

        return reported;
    }

    AnimClipData BuildClip(const RawClip& raw, size_t* outDroppedChannels)
    {
        const double tps = ResolveTicksPerSecond(raw.ticksPerSecond);

        AnimClipData clip;
        clip.name = raw.name;

        // Duration converts with the SAME resolved rate as the keys. Resolving it
        // twice, or resolving it here and using raw.ticksPerSecond for the tracks,
        // puts the loop point somewhere other than the last key.
        clip.duration = static_cast<float>(raw.durationTicks / tps);

        clip.channels.reserve(raw.channels.size());

        size_t dropped = 0;

        for (const RawChannel& rawChannel : raw.channels)
        {
            AnimChannel channel;
            channel.boneName = rawChannel.boneName;

            const bool ok =
                ConvertTrack(rawChannel.posTimes,   rawChannel.posValues,   tps,
                             channel.posTimes,      channel.posValues) &&
                ConvertTrack(rawChannel.rotTimes,   rawChannel.rotValues,   tps,
                             channel.rotTimes,      channel.rotValues) &&
                ConvertTrack(rawChannel.scaleTimes, rawChannel.scaleValues, tps,
                             channel.scaleTimes,    channel.scaleValues);

            // Dropped, not repaired: a channel whose times and values disagree in
            // length would sample as garbage, and silently truncating it hides
            // whatever produced it.
            if (!ok)
            {
                ++dropped;
                continue;
            }

            clip.channels.push_back(std::move(channel));
        }

        if (outDroppedChannels) *outDroppedChannels = dropped;

        return clip;
    }
}
