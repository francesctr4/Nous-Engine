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
    // Append-only: new values go before ALL_TYPES so existing .meta files
    // (which store the integer enum value) keep resolving.
    SCENE,
    VIDEO,
    AUDIO_GRAPH,
    SKELETON,
    ANIMATION,

    ALL_TYPES
};
