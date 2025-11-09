#ifndef TIMEMANAGER_H
#define TIMEMANAGER_H

#include "Timer/include/Timer.h"

namespace TimeManager 
{
    inline Timer graphicsTimer;
    inline Timer gameTimer;

    inline float deltaTime = 0.0f;
    inline int frameCount = 0;
}

#endif // TIMEMANAGER_H