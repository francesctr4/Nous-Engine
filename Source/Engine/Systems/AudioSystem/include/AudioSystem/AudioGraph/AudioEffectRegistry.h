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

// Presentation grouping for an effect. Lives here rather than in the editor because
// it is a property of the effect, not of the window — the editor maps it to colors
// and keeps its own two anchor categories (Source / Output), which are not effects.
enum class AudioEffectCategory : uint8_t
{
    Effect,    // alters the signal's character
    Utility    // alters level / routing only
};

// Everything the engine knows about one effect, in one row.
struct AudioEffectInfo
{
    AudioEffectType                       type;
    const char*                           token;         // stable serialization token
    const char*                           displayName;   // shown in menus and node headers
    AudioEffectCategory                   category;
    std::span<const AudioEffectParamDesc> params;
};

// Single source of truth for "what is an effect": display name, parameter schema,
// defaults, category, and the type<->string mapping used for serialization.
// Header-only and dependency-free so it drives the editor palette, the inline node
// widgets, the importer, and a pure (engine-free) unit test.
//
// ADDING AN EFFECT IS THREE FILES:
//   1. AudioEffectTypes.h   — one enum value (before COUNT)
//   2. this file            — one param table + one row in k_allEffects
//   3. MiniaudioEffectChain.cpp — Create / Destroy / SetParam cases
// The editor is NOT one of them, and t_AudioSystem_AudioEffectRegistry's
// CoversEveryEffectType test fails if step 2 is forgotten.
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
    // verblib's own defaults, except `dry`: it ships 0.0 (fully wet), which is right
    // for a send and wrong for the insert chain this is. Defaulted to 1.0 so dropping
    // a Reverb node ADDS reverb instead of replacing the signal — same reasoning as
    // the Delay node's ma_delay_node_set_dry(1.0).
    inline constexpr AudioEffectParamDesc k_reverbParams[]   = {
        { "roomSize", 0.50f, 0.0f, 1.0f },
        { "damping",  0.25f, 0.0f, 1.0f },
        { "wet",      0.33f, 0.0f, 1.0f },
        { "dry",      1.00f, 0.0f, 1.0f },
        { "width",    1.00f, 0.0f, 1.0f }
    };

    // The registry. Order is the editor's palette order.
    inline constexpr std::array<AudioEffectInfo, 5> k_allEffects = { {
        { AudioEffectType::LowPass,  "LowPass",  "Low Pass",  AudioEffectCategory::Effect,  k_lowPassParams  },
        { AudioEffectType::HighPass, "HighPass", "High Pass", AudioEffectCategory::Effect,  k_highPassParams },
        { AudioEffectType::Delay,    "Delay",    "Delay",     AudioEffectCategory::Effect,  k_delayParams    },
        { AudioEffectType::Gain,     "Gain",     "Gain",      AudioEffectCategory::Utility, k_gainParams     },
        { AudioEffectType::Reverb,   "Reverb",   "Reverb",    AudioEffectCategory::Effect,  k_reverbParams   }
    } };

    // Null only for a type with no row — which the completeness test forbids.
    [[nodiscard]] inline constexpr const AudioEffectInfo* Find(AudioEffectType t)
    {
        for (const AudioEffectInfo& e : k_allEffects)
            if (e.type == t)
                return &e;
        return nullptr;
    }

    [[nodiscard]] inline std::span<const AudioEffectParamDesc> Params(AudioEffectType t)
    {
        const AudioEffectInfo* e = Find(t);
        return e ? e->params : std::span<const AudioEffectParamDesc>{};
    }

    [[nodiscard]] inline const char* DisplayName(AudioEffectType t)
    {
        const AudioEffectInfo* e = Find(t);
        return e ? e->displayName : "Unknown";
    }

    [[nodiscard]] inline AudioEffectCategory Category(AudioEffectType t)
    {
        const AudioEffectInfo* e = Find(t);
        return e ? e->category : AudioEffectCategory::Utility;
    }

    // Stable serialization token (independent of DisplayName, which may change).
    [[nodiscard]] inline const char* TypeToString(AudioEffectType t)
    {
        const AudioEffectInfo* e = Find(t);
        return e ? e->token : "LowPass";
    }

    [[nodiscard]] inline bool TypeFromString(std::string_view s, AudioEffectType& out)
    {
        for (const AudioEffectInfo& e : k_allEffects)
        {
            if (s == e.token)
            {
                out = e.type;
                return true;
            }
        }
        return false;
    }

    [[nodiscard]] inline std::vector<float> DefaultParams(AudioEffectType t)
    {
        std::vector<float> v;
        for (const AudioEffectParamDesc& p : Params(t))
            v.push_back(p.def);
        return v;
    }
}
