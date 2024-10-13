#pragma once

#include "Timer.h"

namespace TimeManager 
{
    inline Timer graphicsTimer;
    inline Timer gameTimer;

    inline float deltaTime = 0.0f;
    inline int frameCount = 0;
} 