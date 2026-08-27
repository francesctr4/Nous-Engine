#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include <TimeManager/Timer.h>

namespace TimeManager
{
    inline Timer graphicsTimer;
    inline Timer gameTimer;

    inline float deltaTime = 0.0f;
    inline int frameCount = 0;

    // Simulation time — driven by ModuleScene simulation state.
    // 0 when stopped or paused; equals dt when playing; exactly one dt tick when stepping.
    inline float simulationDeltaTime  = 0.0f;
    inline float simulationTime       = 0.0f;  // total elapsed since last PressPlay
    inline int   simulationFrameCount = 0;
}

#endif // TIMEMANAGER_H