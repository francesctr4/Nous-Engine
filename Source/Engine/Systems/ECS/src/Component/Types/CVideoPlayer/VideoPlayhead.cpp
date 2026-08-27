#include "Component/Types/CVideoPlayer/VideoPlayhead.h"

#include <cmath>

double AdvanceVideoPlayhead(double current, double dt, double durationSec, bool loop)
{
    double next = current + dt;
    if (next < 0.0) next = 0.0;

    if (durationSec <= 0.0)
        return next;   // unknown duration: just advance

    if (loop)
    {
        next = std::fmod(next, durationSec);
        if (next < 0.0) next += durationSec;
        return next;
    }

    return (next > durationSec) ? durationSec : next;
}

double ResolveVideoPlayhead(double current, double dt, double durationSec, bool loop,
                            bool audioClockActive, double audioSeconds)
{
    if (audioClockActive)
        return (audioSeconds < 0.0) ? 0.0 : audioSeconds;

    return AdvanceVideoPlayhead(current, dt, durationSec, loop);
}
