#include <AnimationSystem/Sampling.h>

#include <AnimationSystem/AnimClip.h>
#include <AnimationSystem/Binding.h>

#include <glm/gtc/quaternion.hpp>

#include <cmath>

namespace nous::engine::animation_system
{
    namespace
    {
        // Interpolates one track given an already-located key pair. Split out so the
        // three tracks share the boundary handling instead of open-coding it three
        // times with three chances to get an edge case wrong.
        template <typename T, typename LerpFn>
        T SampleTrack(const std::vector<float>& times, const std::vector<T>& values,
                      float t, uint32_t& cursor, const T& fallback, LerpFn lerp)
        {
            if (values.empty()) return fallback;   // no keys: bone keeps its bind value
            if (values.size() == 1) return values[0];

            const KeyLocation key = FindKey(times, t, cursor);

            if (key.index + 1 >= values.size()) return values.back();

            return lerp(values[key.index], values[key.index + 1], key.factor);
        }
    }

    KeyLocation FindKey(const std::vector<float>& times, float t, uint32_t& cursor)
    {
        if (times.empty())
        {
            cursor = 0;
            return {};
        }

        size_t i = cursor;
        if (i >= times.size()) i = times.size() - 1;   // stale cursor from a shorter track

        // Backwards seek: the cursor's forward-only premise is broken, so rescan
        // from the start. Self-healing on purpose -- a caller that forgets to reset
        // after a seek gets a slow frame, not wrong output.
        if (t < times[i]) i = 0;

        while (i + 1 < times.size() && times[i + 1] <= t) ++i;

        cursor = static_cast<uint32_t>(i);

        // Last key, or clamped past the end: factor 0 so the caller's unconditional
        // interpolate is a no-op onto values[i].
        if (i + 1 >= times.size()) return { i, 0.0f };

        const float span = times[i + 1] - times[i];

        // Duplicate timestamps are legal in exported clips (a hold, or a step key).
        // Dividing by that span is the division by zero this guard exists for.
        if (span <= 0.0f) return { i, 0.0f };

        // Clamped because t may sit before times[0] -- Sample() does not wrap, so a
        // time below the first key must pin to it rather than extrapolate backwards.
        const float factor = (t - times[i]) / span;
        return { i, factor < 0.0f ? 0.0f : (factor > 1.0f ? 1.0f : factor) };
    }

    void Advance(AnimInstance& instance, float dt)
    {
        if (!instance.clip) return;

        const float duration = instance.clip->duration;
        instance.time += dt * instance.speed;

        if (duration <= 0.0f)
        {
            // A single-pose clip. Anything else here divides by zero in fmod or
            // spins forever in a wrap loop.
            instance.time = 0.0f;
            return;
        }

        if (instance.loop)
        {
            instance.time = std::fmod(instance.time, duration);
            if (instance.time < 0.0f) instance.time += duration;   // fmod keeps the sign

            // O(channels), not O(keys): the wrap is exactly the discontinuity the
            // cursor cannot walk through.
            instance.ResetCursor();
        }
        else
        {
            if (instance.time < 0.0f)          { instance.time = 0.0f;     instance.ResetCursor(); }
            else if (instance.time > duration) { instance.time = duration; }
        }
    }

    bool IsFinished(const AnimInstance& instance)
    {
        if (!instance.clip) return true;
        if (instance.loop)  return false;

        // Duration 0 is finished on arrival; `time >= duration` would also say so,
        // but only because both are 0 -- state it outright rather than lean on that.
        if (instance.clip->duration <= 0.0f) return true;

        return instance.speed >= 0.0f
            ? instance.time >= instance.clip->duration
            : instance.time <= 0.0f;
    }

    void Sample(AnimInstance& instance, const SkeletonData& skeleton, uint32_t skeletonUID,
                Pose& outPose)
    {
        const size_t boneCount = skeleton.BoneCount();

        outPose.skeleton = skeletonUID;
        outPose.bones.resize(boneCount);

        // Start from the bind pose so bones this clip does not touch hold their rest
        // transform. Leaving them default-constructed would collapse every
        // undriven joint onto the origin -- and the symptom (a rig folding in on
        // itself) reads as a skinning bug, not a sampling one.
        const bool haveBindPose = skeleton.bindLocals.size() == boneCount;

        for (size_t b = 0; b < boneCount; ++b)
        {
            outPose.bones[b] = haveBindPose ? skeleton.bindLocals[b] : Transform{};
        }

        if (!instance.clip || !instance.binding) return;

        const AnimClipData& clip = *instance.clip;
        const std::vector<int>& channelToBone = instance.binding->channelToBone;

        if (instance.cursor.size() != clip.channels.size()) instance.ResetCursor();

        const float t = instance.time;

        for (size_t c = 0; c < clip.channels.size(); ++c)
        {
            if (c >= channelToBone.size()) break;   // binding built against another clip

            const int bone = channelToBone[c];
            if (bone < 0 || static_cast<size_t>(bone) >= boneCount) continue;

            const AnimChannel& channel = clip.channels[c];
            Transform& out = outPose.bones[bone];
            glm::uvec3& cursor = instance.cursor[c];

            out.position = SampleTrack(channel.posTimes, channel.posValues, t, cursor.x,
                out.position, [](const glm::vec3& a, const glm::vec3& b, float f)
                {
                    return glm::mix(a, b, f);
                });

            out.rotation = SampleTrack(channel.rotTimes, channel.rotValues, t, cursor.y,
                out.rotation, [](const glm::quat& a, const glm::quat& b, float f)
                {
                    // Short arc, same as Interpolate -- glm::slerp flips the sign of
                    // the target when the dot product is negative.
                    return glm::normalize(glm::slerp(a, b, f));
                });

            out.scale = SampleTrack(channel.scaleTimes, channel.scaleValues, t, cursor.z,
                out.scale, [](const glm::vec3& a, const glm::vec3& b, float f)
                {
                    return glm::mix(a, b, f);
                });
        }
    }
}
