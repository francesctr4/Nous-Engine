#ifndef NOUS_ENGINE_UPDATE_STATUS_H
#define NOUS_ENGINE_UPDATE_STATUS_H

#include <cstdint>

enum class UpdateStatus : uint8_t
{
    CONTINUE = 1,
    STOP = 2,
    ERROR = 3
};

#endif // NOUS_ENGINE_UPDATE_STATUS_H
