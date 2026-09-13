#include <AnimationSystem/AnimationEvents.h>

#include <algorithm>

namespace nous::engine::animation_system
{
    namespace
    {
        // Appends indices with lo <= t < hi (or t <= hi when closeEnd), ASCENDING.
        //
        // Deliberately NOT breaking early on an out-of-range time: that would require
        // the input to be sorted, and a hand-edited stub is not guaranteed to be. An
        // event list is a handful of entries, so the full walk costs nothing.
        void AppendAscending(const std::vector<AnimationEvent>& events,
                             const float lo, const float hi, const bool closeEnd,
                             std::vector<int>& out)
        {
            for (size_t i = 0; i < events.size(); ++i)
            {
                const float t = events[i].time;
                if (t < lo) continue;
                if (closeEnd ? (t > hi) : (t >= hi)) continue;

                out.push_back(static_cast<int>(i));
            }
        }

        // Appends indices with lo < t <= hi (or lo <= t when closeLow), DESCENDING.
        void AppendDescending(const std::vector<AnimationEvent>& events,
                              const float lo, const float hi, const bool closeLow,
                              std::vector<int>& out)
        {
            for (size_t i = events.size(); i-- > 0; )
            {
                const float t = events[i].time;
                if (t > hi) continue;
                if (closeLow ? (t < lo) : (t <= lo)) continue;

                out.push_back(static_cast<int>(i));
            }
        }
    }

    void SortEvents(std::vector<AnimationEvent>& events)
    {
        // STABLE, so two events authored at the same instant keep authoring order --
        // which is the only thing that makes their relative order defined at all.
        std::stable_sort(events.begin(), events.end(),
                         [](const AnimationEvent& a, const AnimationEvent& b)
                         { return a.time < b.time; });
    }

    void CollectFiredEvents(const std::vector<AnimationEvent>& events,
                            const float timeBefore, const float timeAfter,
                            const float duration, const bool wrapped,
                            const bool reversed, const bool finished,
                            std::vector<int>& outIndices)
    {
        if (events.empty() || duration <= 0.0f) return;

        // A zero-width interval is the STOPPED scene (Advance(0) traverses nothing)
        // and the already-clamped non-looping clip. Returning here is what stops rule
        // 2's closed end from re-firing a terminal event on every later frame.
        if (!wrapped && timeBefore == timeAfter) return;

        if (!reversed)
        {
            if (!wrapped)
            {
                AppendAscending(events, timeBefore, timeAfter, finished, outIndices);
                return;
            }

            // The loop seam, split exactly as ComputeRootDelta splits it. The far
            // side's end is clamped against timeBefore so a frame covering more than
            // one cycle cannot fire an event twice (rule 5).
            AppendAscending(events, timeBefore, duration, true, outIndices);
            AppendAscending(events, 0.0f, std::min(timeAfter, timeBefore), false, outIndices);
            return;
        }

        if (!wrapped)
        {
            AppendDescending(events, timeAfter, timeBefore, finished, outIndices);
            return;
        }

        // Reverse crosses the seam downwards: through 0 first (closed, because 0 is
        // traversed), then back from duration. Same rule-5 clamp, mirrored.
        AppendDescending(events, 0.0f, timeBefore, true, outIndices);
        AppendDescending(events, std::max(timeAfter, timeBefore), duration, false, outIndices);
    }
}
