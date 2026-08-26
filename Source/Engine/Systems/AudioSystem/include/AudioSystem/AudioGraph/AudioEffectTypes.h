#pragma once

#include <cstdint>
#include <vector>

// Backend-agnostic audio-effect "recipe". No miniaudio, no ECS, no imgui — so the
// resource layer, the backend interface, and the editor can all name these types
// without pulling in any backend. (Same dependency-free policy as SoundHandle.h /
// AudioTypes.h.) Grows by one enum value per new effect.
enum class AudioEffectType : uint8_t
{
    LowPass,
    HighPass,
    Delay,
    Gain
};

// One effect instance in a chain. `params` is parallel to this type's schema in
// AudioEffectRegistry (same order, same count).
struct AudioEffectDesc
{
    AudioEffectType    type{};
    std::vector<float> params;
};

// An ordered effect chain. Vector order == signal order (linear chain).
using AudioGraphDesc = std::vector<AudioEffectDesc>;
