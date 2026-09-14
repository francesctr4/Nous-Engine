#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

/**
 * @brief Pure time<->pixel arithmetic for the animation event timeline.
 *
 * ImGui-free, runtime-free, fully inline: the AudioGraphLinearize split. The window
 * that consumes this is untestable (it needs a live editor and a renderer), so
 * everything that can be decided without drawing lives here instead, and the tests
 * drive the SAME code the editor runs.
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
