#pragma once

#include <cstdint>

struct TimeAPI
{
    float    (*GetDeltaTime)()           = nullptr; // real frame delta (always ticking)
    float    (*GetSimDeltaTime)()        = nullptr; // 0 when stopped/paused, equals dt when playing
    float    (*GetElapsedTime)()         = nullptr; // total simulation time since last PressPlay
    int      (*GetFrameCount)()          = nullptr; // total frames since engine start
    int      (*GetSimFrameCount)()       = nullptr; // frames since last PressPlay
};

void SetupTimeBindings(TimeAPI& time);
