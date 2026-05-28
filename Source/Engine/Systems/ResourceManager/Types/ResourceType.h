#pragma once

#include <cstdint>

enum class ResourceType : int8_t
{
    UNKNOWN = -1,

    MESH,
    MATERIAL,
    TEXTURE,
    SHADER,
    AUDIO,

    ALL_TYPES
};
