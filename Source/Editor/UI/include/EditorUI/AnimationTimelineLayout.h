#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

/**
 * @brief Pure time<->pixel arithmetic for the animation event timeline.
 *
 * ImGui-free, runtime-free, fully inline: the AudioGraphLinearize split. The window that
 * consumes it needs a live editor and a renderer to run, so everything decidable without
 * drawing lives here and the tests drive the SAME code the editor runs.
 */
namespace nous::editor::timeline
{
    // The drawn ruler: where it starts on screen, how wide it is, and the clip it spans.
    struct RulerLayout
    {
        float x        = 0.0f;
        float width    = 0.0f;
        float duration = 0.0f;
    };

    // A degenerate ruler collapses to its start rather than dividing by zero: a clip
    // whose Library/ entry failed to load has duration 0, and the window still draws.
    [[nodiscard]] inline float TimeToX(const RulerLayout& ruler, const float time)
    {
        if (ruler.duration <= 0.0f) return ruler.x;
        return ruler.x + ruler.width * (time / ruler.duration);
    }

    [[nodiscard]] inline float XToTime(const RulerLayout& ruler, const float x)
    {
        if (ruler.width <= 0.0f) return 0.0f;
        return ruler.duration * ((x - ruler.x) / ruler.width);
    }

    // Clamped to [0, duration), never [0, duration]: a looping clip wraps AT duration,
    // so an event authored exactly there is only reachable through the seam's closed
    // end and is better expressed just before it. The epsilon is relative to the clip
    // so it stays meaningful for a 0.1 s clip and a 100 s one alike.
    [[nodiscard]] inline float ClampTime(const float time, const float duration)
    {
        if (duration <= 0.0f) return 0.0f;

        const float last = duration - duration * 1e-4f;
        return std::clamp(time, 0.0f, last);
    }

    // `snapPerSecond` is a grid in steps per second (30 == thirtieths). NON-POSITIVE
    // MEANS OFF -- 0, not 1, because a one-step-per-second grid is a legitimate coarse
    // setting a user may pick.
    [[nodiscard]] inline float SnapTime(const float time, const float snapPerSecond)
    {
        if (snapPerSecond <= 0.0f) return time;
        return std::round(time * snapPerSecond) / snapPerSecond;
    }

    // Seconds between LABELLED ticks on the ruler, or 0 for "draw no grid".
    //
    // Two constraints: a step must be ROUND enough to read as a label (the 1-2-5 ladder,
    // so never 0.3333 s) and WIDE enough that its label clears its neighbour's.
    //
    // Derived from the ruler rather than the snap setting, because snapping is about where
    // a marker may LAND -- a different question from what the ruler can legibly say. A
    // 30/s snap over a 2 s clip is 60 identical unlabelled marks.
    [[nodiscard]] inline float ChooseTickStep(const float duration, const float width,
                                              const float minSpacingPx)
    {
        if (duration <= 0.0f || width <= 0.0f) return 0.0f;

        // The narrowest step that still clears the spacing, before rounding it to a
        // readable number. Rounding can only go UP from here, so the constraint holds.
        const float minStep = duration * (minSpacingPx / width);
        const float decade  = std::pow(10.0f, std::floor(std::log10(minStep)));

        for (const float mantissa : { 1.0f, 2.0f, 5.0f })
            if (decade * mantissa >= minStep) return decade * mantissa;

        return decade * 10.0f;   // next decade's 1, e.g. minStep 6 -> 10
    }

    // Index of the marker nearest `mouseX` within `grabRadius` pixels, or -1.
    //
    // NEAREST rather than first-within-radius: markers routinely sit within a few
    // pixels of each other, and picking the first would make dragging the one you
    // clicked a coin flip.
    [[nodiscard]] inline int HitTestMarker(const RulerLayout& ruler,
                                           const std::vector<float>& times,
                                           const float mouseX, const float grabRadius)
    {
        int   best         = -1;
        float bestDistance = grabRadius;

        for (size_t i = 0; i < times.size(); ++i)
        {
            const float distance = std::abs(TimeToX(ruler, times[i]) - mouseX);
            if (distance <= bestDistance)
            {
                bestDistance = distance;
                best         = static_cast<int>(i);
            }
        }

        return best;
    }
}
