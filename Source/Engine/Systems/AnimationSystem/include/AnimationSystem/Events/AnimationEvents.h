#pragma once

#include <string>
#include <vector>

namespace nous::engine::animation_system
{
    /**
     * @brief A named marker at a time in a clip.
     *
     * Here only so the collector below can be tested headless -- the storage belongs to
     * ResourceAnimation, which composes a vector of these by value. Events are authoring
     * metadata, so AnimClipData deliberately does not carry them: this library stays about
     * sampling.
     */
    struct AnimationEvent
    {
        float       time = 0.0f;      // SECONDS into the clip, like every other time here
        std::string name;             // what a script switches on
        float       floatParam = 0.0f;
        std::string stringParam;
    };

    // Stable sort by time, so two events at the same instant keep their authoring order.
    // Called by the WRITER, never per frame -- the collector never breaks early, so an
    // unsorted clip still yields the right set and only their emission order follows.
    void SortEvents(std::vector<AnimationEvent>& events);

    /**
     * @brief Appends the indices of every event in the interval the clip traversed.
     *
     * `timeBefore` is the instance's time BEFORE Advance, `timeAfter` after it; `wrapped`
     * is Advance's return value; `reversed` means the composed rate is negative;
     * `finished` means a non-looping clip has clamped at an end.
     *
     * The interval is [before, after): closed at the start so an event at t = 0 fires on
     * the first frame, open at the end so two consecutive frames cannot both fire the same
     * event at their boundary. A non-looping clip's final frame closes it, since Advance
     * clamps at duration -- but ONLY while the interval is non-empty, or that event would
     * re-fire every frame thereafter.
     *
     * A wrap splits at the seam as ComputeRootDelta does: [before, duration] then
     * [0, after). Reverse mirrors both the interval and the emission order -- (after,
     * before] descending, and a reverse wrap splits [0, before] then (after, duration].
     * The second segment is clamped against the first so each event fires at most once
     * per frame, since a frame longer than the clip wraps once through fmod.
     *
     * Appends nothing for an empty list, a non-positive duration, or a zero-width interval
     * -- the last of which is what makes a STOPPED scene and a finished non-looping clip
     * both fire nothing, with no sim-state query anywhere.
     */
    void CollectFiredEvents(const std::vector<AnimationEvent>& events,
                            float timeBefore, float timeAfter,
                            float duration, bool wrapped, bool reversed, bool finished,
                            std::vector<int>& outIndices);
}
