#include "Random.h"

#include <random>
#include <limits>

uint64 Random::Generate()
{
    thread_local static std::random_device rd;
    thread_local static std::mt19937 gen(rd());

    thread_local static std::uniform_int_distribution<uint64> distribution(1, std::numeric_limits<uint64>::max());

    return distribution(gen);
}

uint64 Random::GenerateInterval(uint64 min, uint64 max) {

    thread_local static std::random_device rd;
    thread_local static std::mt19937 gen(rd());

    std::uniform_int_distribution<uint64> distribution(min, max);

    return distribution(gen);
}
