#include <gtest/gtest.h>

#include <AudioSystem/AudioGraph/AudioEffectRegistry.h>

// Pure schema tests — no miniaudio, no engine link (header-only registry).

namespace ae = nous::audio;

TEST(t_AudioEffectRegistry, ParamCountsMatchSchema)
{
    EXPECT_EQ(ae::Params(AudioEffectType::LowPass).size(),  1u);
    EXPECT_EQ(ae::Params(AudioEffectType::HighPass).size(), 1u);
    EXPECT_EQ(ae::Params(AudioEffectType::Delay).size(),    3u);
    EXPECT_EQ(ae::Params(AudioEffectType::Gain).size(),     1u);
}

TEST(t_AudioEffectRegistry, DefaultParamsUseSchemaDefaults)
{
    const std::vector<float> delay = ae::DefaultParams(AudioEffectType::Delay);
    ASSERT_EQ(delay.size(), 3u);
    EXPECT_FLOAT_EQ(delay[0], 0.25f);  // delay
    EXPECT_FLOAT_EQ(delay[1], 0.40f);  // decay
    EXPECT_FLOAT_EQ(delay[2], 0.50f);  // wet
}

TEST(t_AudioEffectRegistry, TypeStringRoundTripForAllEffects)
{
    for (AudioEffectType t : ae::k_allEffects)
    {
        AudioEffectType parsed{};
        ASSERT_TRUE(ae::TypeFromString(ae::TypeToString(t), parsed));
        EXPECT_EQ(parsed, t);
    }
}

TEST(t_AudioEffectRegistry, TypeFromStringRejectsUnknown)
{
    AudioEffectType parsed{};
    EXPECT_FALSE(ae::TypeFromString("Bogus", parsed));
}
