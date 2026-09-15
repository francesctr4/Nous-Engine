#pragma once

#include <string>
#include <vector>

namespace nous::engine::animation_system
{
    /**
     * @brief A named marker at a time in a clip.
     *
     * Lives in the PURE layer only so the collector below can be tested headless --
     * the storage belongs to ResourceAnimation, which composes a vector of these by
     * value exactly as it composes AnimClipData and owns AnimationSettings. Events
     * are authoring metadata, so AnimClipData deliberately does NOT carry them:
     * this library stays about sampling.
     */
    struct AnimationEvent
    {
        float       time = 0.0f;      // SECONDS into the clip, like every other time here
        std::string name;             // what a script switches on
        float       floatParam = 0.0f;
        std::string stringParam;
    };

    // Stable sort by time, so two events authored at the same instant keep their
    // authoring order. Called by the WRITER, never per frame: the collector does not
    // require sorted input (it never breaks early), so an unsorted stub yields the
    // right set of events and only their emission order follows the vector.
    void SortEvents(std::vector<AnimationEvent>& events);

    /**
     * @brief Appends the indices of every event in the interval the clip traversed.
     *
     * `timeBefore` is the instance's time BEFORE Advance, `timeAfter` after it;
     * `wrapped` is Advance's return value; `reversed` means the composed playback
     * rate is negative; `finished` means a non-looping clip has clamped at an end.
     *
     * Five rules, each a failure avoided rather than a preference:
     *
     *  1. The interval is [before, after) -- closed at the start so an event at
     *     t = 0 fires on the state's first frame, open at the end so two
     *     consecutive frames cannot both fire the same event at their boundary.
     *  2. A non-looping clip's final frame closes it ([before, after]). Advance
     *     clamps at duration, so an event authored at the very end is otherwise
     *     unreachable forever. Only when the interval is non-empty -- once clamped,
     *     before == after on every later frame, and a closed empty interval would
     *     re-fire that event every frame for the rest of the session.
     *  3. A wrap splits at the seam exactly as ComputeRootDelta splits it:
     *     [before, duration] then [0, after). Getting this wrong drops the event
     *     nearest the loop point, which presents as "footsteps sometimes skip".
     *  4. Reverse reverses the interval AND the emission order: (after, before],
     *     descending -- rule 1 mirrored, closed at the side time starts from. A
     *     reverse wrap splits the other way: [0, before] then (after, duration].
     *  5. Each event fires at most once per frame. A frame longer than the clip
     *     wraps once through fmod; replaying every skipped cycle would emit a burst
     *     of footsteps in one frame. The second segment is clamped against the
     *     first to enforce it.
     *
     * Appends nothing for an empty list, a non-positive duration, or a zero-width
     * interval -- the last of which is what makes a STOPPED scene (simDt == 0) and a
     * finished non-looping clip both fire nothing, with no sim-state query anywhere.
     */
    void CollectFiredEvents(const std::vector<AnimationEvent>& events,
                            float timeBefore, float timeAfter,
                            float duration, bool wrapped, bool reversed, bool finished,
                            std::vector<int>& outIndices);
}
