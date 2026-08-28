#pragma once

#include <EngineCore/EngineExport.h>

// Advance a video playhead by dt (seconds). Non-looping: clamps to [0, durationSec].
// Looping: wraps into [0, durationSec) (a wrap past the end resumes from the start).
// durationSec <= 0 is treated as "unknown" and the playhead is simply advanced by dt
// (never negative). FFmpeg-free, deterministic, unit-tested.
[[nodiscard]] NOUS_ENGINE_API double AdvanceVideoPlayhead(double current, double dt, double durationSec, bool loop);

// Choose the next video playhead. When an audio master clock is active, the video
// follows it exactly (clamped to >= 0). Otherwise it advances on its own dt clock
// via AdvanceVideoPlayhead. FFmpeg-free, deterministic, unit-tested.
[[nodiscard]] NOUS_ENGINE_API double ResolveVideoPlayhead(
    double current, double dt, double durationSec, bool loop,
    bool audioClockActive, double audioSeconds);
