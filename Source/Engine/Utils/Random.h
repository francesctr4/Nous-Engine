#ifndef RANDOM_H
#define RANDOM_H

#include <Engine/Core/Globals.h>

namespace Random 
{
    uint64 Generate();

    uint64 GenerateInterval(uint64 min, uint64 max);
}

#endif // RANDOM_H