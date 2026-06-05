#pragma once

#include <cstdint>

enum class MemoryTag : uint8_t
{
    UNKNOWN = 0,

    THREAD,
    ARRAY,
    DICT,
    APPLICATION,
    RENDERER,
    SCENE,
    GAMEOBJECT,
    SCRIPTING_SYSTEM,
    CAMERA,
    COMPONENT,
    INPUT,
    LINEAR_ALLOCATOR,
    FILE,
    RESOURCE_MESH,
    RESOURCE_TEXTURE,
    RESOURCE_MATERIAL,
    RESOURCE_SHADER,
    RESOURCE_AUDIO,
    RESOURCE_VIDEO,
    EDITOR,
    AUDIO_SYSTEM,

    MAX
};
