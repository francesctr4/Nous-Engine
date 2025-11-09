#ifndef RANDOM_H
#define RANDOM_H

#include <cstdint>

namespace Random
{
    uint64_t Generate();

    uint64_t GenerateInterval(uint64_t min, uint64_t max);
}

#endif // RANDOM_H