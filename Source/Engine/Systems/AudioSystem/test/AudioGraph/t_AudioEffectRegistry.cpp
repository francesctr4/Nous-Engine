#include <gtest/gtest.h>

#include <AudioSystem/AudioGraph/AudioEffectRegistry.h>

#include <cstring>
#include <set>
#include <string_view>

// Pure schema tests — no miniaudio, no engine link (header-only registry).

namespace ae = nous::audio;

// ---------------------------------------------------------------------------
// Completeness — the test that makes "adding an effect is three files" MECHANICAL
// rather than asserted. Everything below it tests values named by hand, so a
// half-added effect would sail past them.
// ---------------------------------------------------------------------------

TEST(t_AudioEffectRegistry, CoversEveryEffectType)
{
    // Every enum value before COUNT must have exactly one row. This is what fails
    // when someone adds an AudioEffectType and forgets the registry table.
    EXPECT_EQ(ae::k_allEffects.size(), static_cast<size_t>(AudioEffectType::COUNT));

    for (uint8_t i = 0; i < static_cast<uint8_t>(AudioEffectType::COUNT); ++i)
    {
        const auto t = static_cast<AudioEffectType>(i);
        EXPECT_NE(ae::Find(t), nullptr) << "no registry row for effect type " << int(i);
    }
}

TEST(t_AudioEffectRegistry, EveryEntryIsWellFormed)
{
    for (const AudioEffectInfo& e : ae::k_allEffects)
    {
        const int id = static_cast<int>(e.type);

        ASSERT_NE(e.token, nullptr)       << "null token for effect " << id;
        ASSERT_NE(e.displayName, nullptr) << "null display name for effect " << id;
        EXPECT_GT(std::strlen(e.token), 0u)       << "empty token for effect " << id;
        EXPECT_GT(std::strlen(e.displayName), 0u) << "empty display name for effect " << id;

        // An effect with no tunable parameter draws an empty node body — allowed,
        // but every parameter it DOES declare must be usable by a slider.
        for (const AudioEffectParamDesc& p : e.params)
        {
            ASSERT_NE(p.name, nullptr) << "null param name in effect " << id;
            EXPECT_GT(std::strlen(p.name), 0u) << "empty param name in effect " << id;
            EXPECT_LT(p.min, p.max) << "param '" << p.name << "' of effect " << id
                                    << " has an empty range";
            EXPECT_GE(p.def, p.min) << "default of '" << p.name << "' is below min";
            EXPECT_LE(p.def, p.max) << "default of '" << p.name << "' is above max";
        }

        EXPECT_TRUE(e.category == AudioEffectCategory::Effect
                 || e.category == AudioEffectCategory::Utility);
    }
}

TEST(t_AudioEffectRegistry, TokensAndDisplayNamesAreUnique)
{
    // A duplicated token silently makes TypeFromString resolve to the wrong effect
    // on load — the asset would deserialize into a different node than it saved.
    std::set<std::string_view> tokens;
    std::set<std::string_view> names;
    for (const AudioEffectInfo& e : ae::k_allEffects)
    {
        EXPECT_TRUE(tokens.insert(e.token).second)      << "duplicate token: "  << e.token;
        EXPECT_TRUE(names.insert(e.displayName).second) << "duplicate name: "   << e.displayName;
    }
}

TEST(t_AudioEffectRegistry, TypeStringRoundTripForAllEffects)
{
    for (const AudioEffectInfo& e : ae::k_allEffects)
    {
        AudioEffectType parsed{};
        ASSERT_TRUE(ae::TypeFromString(ae::TypeToString(e.type), parsed))
            << "token did not parse back: " << e.token;
        EXPECT_EQ(parsed, e.type);
    }
}

TEST(t_AudioEffectRegistry, DefaultParamsMatchSchemaForAllEffects)
{
    for (const AudioEffectInfo& e : ae::k_allEffects)
    {
        const std::vector<float> defaults = ae::DefaultParams(e.type);
        ASSERT_EQ(defaults.size(), e.params.size()) << "effect " << e.token;
        for (size_t i = 0; i < defaults.size(); ++i)
            EXPECT_FLOAT_EQ(defaults[i], e.params[i].def) << "effect " << e.token << " param " << i;
    }
}

// ---------------------------------------------------------------------------
// Specific values — these pin the schema the backend's param indices assume.
// ---------------------------------------------------------------------------

TEST(t_AudioEffectRegistry, ParamCountsMatchSchema)
{
    EXPECT_EQ(ae::Params(AudioEffectType::LowPass).size(),  1u);
    EXPECT_EQ(ae::Params(AudioEffectType::HighPass).size(), 1u);
    EXPECT_EQ(ae::Params(AudioEffectType::Delay).size(),    3u);
    EXPECT_EQ(ae::Params(AudioEffectType::Gain).size(),     1u);
    EXPECT_EQ(ae::Params(AudioEffectType::Reverb).size(),   5u);
}

TEST(t_AudioEffectRegistry, DefaultParamsUseSchemaDefaults)
{
    const std::vector<float> delay = ae::DefaultParams(AudioEffectType::Delay);
    ASSERT_EQ(delay.size(), 3u);
    EXPECT_FLOAT_EQ(delay[0], 0.25f);  // delay
    EXPECT_FLOAT_EQ(delay[1], 0.40f);  // decay
    EXPECT_FLOAT_EQ(delay[2], 0.50f);  // wet
}

TEST(t_AudioEffectRegistry, ReverbDefaultsToAnInsertNotASend)
{
    // verblib's own default dry is 0.0 (fully wet). An insert node must pass the
    // original signal through, so the registry overrides it — if this flips back
    // to 0, dropping a Reverb node silently mutes the dry voice.
    const std::vector<float> reverb = ae::DefaultParams(AudioEffectType::Reverb);
    ASSERT_EQ(reverb.size(), 5u);
    EXPECT_FLOAT_EQ(reverb[3], 1.0f) << "reverb 'dry' default must keep the source audible";
    EXPECT_GT(reverb[2], 0.0f)       << "reverb 'wet' default must be audible";
}

TEST(t_AudioEffectRegistry, CategoriesSplitEffectsFromUtilities)
{
    EXPECT_EQ(ae::Category(AudioEffectType::Gain),   AudioEffectCategory::Utility);
    EXPECT_EQ(ae::Category(AudioEffectType::Reverb), AudioEffectCategory::Effect);
    EXPECT_EQ(ae::Category(AudioEffectType::Delay),  AudioEffectCategory::Effect);
}

TEST(t_AudioEffectRegistry, TypeFromStringRejectsUnknown)
{
    AudioEffectType parsed{};
    EXPECT_FALSE(ae::TypeFromString("Bogus", parsed));
    EXPECT_FALSE(ae::TypeFromString("", parsed));
}
