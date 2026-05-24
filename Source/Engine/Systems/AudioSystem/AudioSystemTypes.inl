#pragma once

#include <cstdint>
#include <string>

enum class AudioEngineBackend : std::int8_t
{
    UNKNOWN = -1,
    MINIAUDIO = 0
};

enum class AudioFileType : std::int8_t
{
    UNKNOWN = -1,
    WAV = 0,
    OGG = 1
};

// Temporary to not depend on Resource Manager yet.
struct ResourceAudio
{
    std::string name;
    AudioFileType type;
    std::string assetsPath;
};