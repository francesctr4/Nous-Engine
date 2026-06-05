#pragma once

#include "Engine/EngineExport.h"

// Advance a video playhead by dt (seconds). Non-looping: clamps to [0, durationSec].
// Looping: wraps into [0, durationSec) (a wrap past the end resumes from the start).
// durationSec <= 0 is treated as "unknown" and the playhead is simply advanced by dt
// (never negative). FFmpeg-free, deterministic, unit-tested.
[[nodiscard]] NOUS_ENGINE_API double AdvanceVideoPlayhead(double current, double dt, double durationSec, bool loop);
