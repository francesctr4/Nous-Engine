#pragma once
#include <cstdint>

enum class UpdateStatus : uint8_t
{
    CONTINUE = 1,
    STOP = 2,
    ERROR = 3
};

