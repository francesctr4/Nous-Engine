#include <Engine/Utils/Serialization/Random/Random.h>

#include <random>
#include <limits>

uint64_t Random::Generate()
{
    thread_local static std::random_device rd;
    thread_local static std::mt19937 gen(rd());

    thread_local static std::uniform_int_distribution<uint64_t> distribution(1, std::numeric_limits<uint64_t>::max());

    return distribution(gen);
}

uint64_t Random::GenerateInterval(uint64_t min, uint64_t max) {

    thread_local static std::random_device rd;
    thread_local static std::mt19937 gen(rd());

    std::uniform_int_distribution<uint64_t> distribution(min, max);

    return distribution(gen);
}
