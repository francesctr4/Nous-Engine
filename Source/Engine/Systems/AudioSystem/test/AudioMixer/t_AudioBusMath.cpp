#include <gtest/gtest.h>

#include "AudioMixer/AudioBusMath.h"

// Pure mixer-math tests for the bus policy — no miniaudio, no device. The miniaudio
// realization (MiniaudioBusGraph) reuses ComputeEffectiveGain() verbatim.

TEST(t_AudioBusMath, NoSolo_Unmuted_PassesUserVolume)
{
    BusState bus{ 0.5f, false, false };
    EXPECT_FLOAT_EQ(ComputeEffectiveGain(bus, /*anySolo=*/false), 0.5f);
}

TEST(t_AudioBusMath, Muted_IsSilent)
{
    BusState bus{ 0.8f, true, false };
    EXPECT_FLOAT_EQ(ComputeEffectiveGain(bus, false), 0.0f);
}

TEST(t_AudioBusMath, OtherBusSoloed_NonSoloedBus_IsSilent)
{
    BusState bus{ 1.0f, false, /*solo=*/false };
    EXPECT_FLOAT_EQ(ComputeEffectiveGain(bus, /*anySolo=*/true), 0.0f);
}

TEST(t_AudioBusMath, Soloed_WhileSoloActive_PassesUserVolume)
{
    BusState bus{ 0.7f, false, /*solo=*/true };
    EXPECT_FLOAT_EQ(ComputeEffectiveGain(bus, /*anySolo=*/true), 0.7f);
}

TEST(t_AudioBusMath, MuteWinsOverSolo)
{
    BusState bus{ 1.0f, /*mute=*/true, /*solo=*/true };
    EXPECT_FLOAT_EQ(ComputeEffectiveGain(bus, /*anySolo=*/true), 0.0f);
}
