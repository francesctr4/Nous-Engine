#include <gtest/gtest.h>

#include "AudioGraph/GainDsp.h"

// Pure DSP: the one piece of effect math we author (stock ma_nodes are trusted).

TEST(t_GainDsp, ScalesAllSamples)
{
    const float in[4]  = { 0.5f, -0.5f, 1.0f, -1.0f };
    float       out[4] = {};
    GainApply(out, in, /*frameCount=*/2, /*channels=*/2, /*gain=*/2.0f);
    EXPECT_FLOAT_EQ(out[0],  1.0f);
    EXPECT_FLOAT_EQ(out[1], -1.0f);
    EXPECT_FLOAT_EQ(out[2],  2.0f);
    EXPECT_FLOAT_EQ(out[3], -2.0f);
}

TEST(t_GainDsp, ZeroGainSilences)
{
    const float in[2]  = { 0.9f, -0.3f };
    float       out[2] = {};
    GainApply(out, in, /*frameCount=*/1, /*channels=*/2, /*gain=*/0.0f);
    EXPECT_FLOAT_EQ(out[0], 0.0f);
    EXPECT_FLOAT_EQ(out[1], 0.0f);
}
