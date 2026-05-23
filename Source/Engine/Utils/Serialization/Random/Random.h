#ifndef RANDOM_H
#define RANDOM_H

#include "Engine/EngineExport.h"
#include <cstdint>

namespace Random
{
    NOUS_ENGINE_API uint64_t Generate();

    NOUS_ENGINE_API uint64_t GenerateInterval(uint64_t min, uint64_t max);
}

#endif // RANDOM_H