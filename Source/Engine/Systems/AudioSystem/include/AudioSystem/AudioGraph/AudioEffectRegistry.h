#pragma once

#include <AudioSystem/AudioGraph/AudioEffectTypes.h>

#include <array>
#include <span>
#include <string_view>
#include <vector>

// Schema entry for one tunable parameter of an effect.
struct AudioEffectParamDesc
{
    const char* name;
    float       def;
    float       min;
    float       max;
};

// Single source of truth for "what is an effect": display name, parameter schema,
// defaults, and the type<->string mapping used for serialization. Header-only and
// dependency-free so it drives the editor palette, the inline node widgets, the
// importer, and a pure (engine-free) unit test.
//
// To add an effect: add an AudioEffectType value (AudioEffectTypes.h), a schema
// table + cases below, and append it to k_allEffects. (The third "touch" is the
// backend factory — Layer 2.)
namespace nous::audio
{
    inline constexpr AudioEffectParamDesc k_lowPassParams[]  = {
        { "cutoff", 1000.0f, 20.0f, 20000.0f }
    };
    inline constexpr AudioEffectParamDesc k_highPassParams[] = {
        { "cutoff", 200.0f, 20.0f, 20000.0f }
    };
    inline constexpr AudioEffectParamDesc k_delayParams[]    = {
        { "delay", 0.25f, 0.0f, 2.0f  },
        { "decay", 0.40f, 0.0f, 0.95f },
        { "wet",   0.50f, 0.0f, 1.0f  }
    };
    inline constexpr AudioEffectParamDesc k_gainParams[]     = {
        { "gain", 1.0f, 0.0f, 2.0f }
    };

    [[nodiscard]] inline std::span<const AudioEffectParamDesc> Params(AudioEffectType t)
    {
        switch (t)
        {
            case AudioEffectType::LowPass:  return k_lowPassParams;
            case AudioEffectType::HighPass: return k_highPassParams;
            case AudioEffectType::Delay:    return k_delayParams;
            case AudioEffectType::Gain:     return k_gainParams;
        }
        return {};
    }

    [[nodiscard]] inline const char* DisplayName(AudioEffectType t)
    {
        switch (t)
        {
            case AudioEffectType::LowPass:  return "Low Pass";
            case AudioEffectType::HighPass: return "High Pass";
            case AudioEffectType::Delay:    return "Delay";
            case AudioEffectType::Gain:     return "Gain";
        }
        return "Unknown";
    }

    // Stable serialization token (independent of DisplayName, which may change).
    [[nodiscard]] inline const char* TypeToString(AudioEffectType t)
    {
        switch (t)
        {
            case AudioEffectType::LowPass:  return "LowPass";
            case AudioEffectType::HighPass: return "HighPass";
            case AudioEffectType::Delay:    return "Delay";
            case AudioEffectType::Gain:     return "Gain";
        }
        return "LowPass";
    }

    [[nodiscard]] inline bool TypeFromString(std::string_view s, AudioEffectType& out)
    {
        if (s == "LowPass")  { out = AudioEffectType::LowPass;  return true; }
        if (s == "HighPass") { out = AudioEffectType::HighPass; return true; }
        if (s == "Delay")    { out = AudioEffectType::Delay;    return true; }
        if (s == "Gain")     { out = AudioEffectType::Gain;     return true; }
        return false;
    }

    [[nodiscard]] inline std::vector<float> DefaultParams(AudioEffectType t)
    {
        std::vector<float> v;
        for (const auto& p : Params(t))
            v.push_back(p.def);
        return v;
    }

    // Palette order — drives the editor's "add node" menu (Layer 3).
    inline constexpr std::array<AudioEffectType, 4> k_allEffects = {
        AudioEffectType::LowPass,
        AudioEffectType::HighPass,
        AudioEffectType::Delay,
        AudioEffectType::Gain
    };
}
