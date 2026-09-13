#include <ResourceManager/Import/ModelParser/ClipBuild.h>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>

using namespace nous::engine::resource_manager;

namespace
{
    constexpr float kEps = 1e-5f;

    RawChannel PositionChannel(std::string boneName, std::vector<double> ticks)
    {
        RawChannel channel;
        channel.boneName = std::move(boneName);
        channel.posTimes = std::move(ticks);
        channel.posValues.assign(channel.posTimes.size(), glm::vec3(0.0f));
        return channel;
    }
}

// ---------------------------------------------------------------------------
// ResolveTicksPerSecond -- the classic FBX gotcha
// ---------------------------------------------------------------------------

TEST(t_ClipBuild, ReportedRateIsUsedWhenValid)
{
    EXPECT_DOUBLE_EQ(ResolveTicksPerSecond(30.0),   30.0);
    EXPECT_DOUBLE_EQ(ResolveTicksPerSecond(24.0),   24.0);
    EXPECT_DOUBLE_EQ(ResolveTicksPerSecond(4800.0), 4800.0);
}

// FBX exporters routinely write 0 here and assimp passes it straight through.
// Dividing by it is an infinity; treating it as 1 makes a 24 fps clip play 24x
// too slow, which reads as an animation bug rather than an import bug.
TEST(t_ClipBuild, ZeroRateFallsBackToAssimpDefault)
{
    EXPECT_DOUBLE_EQ(ResolveTicksPerSecond(0.0), 25.0);
}

TEST(t_ClipBuild, NegativeAndNonFiniteRatesFallBack)
{
    EXPECT_DOUBLE_EQ(ResolveTicksPerSecond(-30.0), 25.0);
    EXPECT_DOUBLE_EQ(ResolveTicksPerSecond(std::numeric_limits<double>::quiet_NaN()), 25.0);
    EXPECT_DOUBLE_EQ(ResolveTicksPerSecond(std::numeric_limits<double>::infinity()), 25.0);
}

// ---------------------------------------------------------------------------
// BuildClip
// ---------------------------------------------------------------------------

TEST(t_ClipBuild, KeyTimesConvertToSeconds)
{
    RawClip raw;
    raw.name           = "Walk";
    raw.ticksPerSecond = 24.0;
    raw.durationTicks  = 48.0;
    raw.channels       = { PositionChannel("Root", { 0.0, 12.0, 24.0, 48.0 }) };

    const auto clip = BuildClip(raw);

    ASSERT_EQ(clip.ChannelCount(), 1u);
    ASSERT_EQ(clip.channels[0].posTimes.size(), 4u);

    EXPECT_NEAR(clip.channels[0].posTimes[0], 0.0f, kEps);
    EXPECT_NEAR(clip.channels[0].posTimes[1], 0.5f, kEps);
    EXPECT_NEAR(clip.channels[0].posTimes[2], 1.0f, kEps);
    EXPECT_NEAR(clip.channels[0].posTimes[3], 2.0f, kEps);
}

TEST(t_ClipBuild, DurationConvertsToSeconds)
{
    RawClip raw;
    raw.ticksPerSecond = 30.0;
    raw.durationTicks  = 90.0;

    EXPECT_NEAR(BuildClip(raw).duration, 3.0f, kEps);
}

// Duration and keys must resolve through the SAME rate. Resolving the fallback
// for one and not the other puts the loop point somewhere other than the last key.
TEST(t_ClipBuild, DurationAndKeysUseTheSameResolvedRate)
{
    RawClip raw;
    raw.ticksPerSecond = 0.0;      // falls back to 25
    raw.durationTicks  = 50.0;
    raw.channels       = { PositionChannel("Root", { 0.0, 50.0 }) };

    const auto clip = BuildClip(raw);

    EXPECT_NEAR(clip.duration, 2.0f, kEps);
    EXPECT_NEAR(clip.channels[0].posTimes.back(), clip.duration, kEps);
}

TEST(t_ClipBuild, NameAndValuesSurviveUnchanged)
{
    RawChannel channel;
    channel.boneName  = "Spine";
    channel.posTimes  = { 0.0, 10.0 };
    channel.posValues = { { 1.0f, 2.0f, 3.0f }, { 4.0f, 5.0f, 6.0f } };

    RawClip raw;
    raw.name           = "Run";
    raw.ticksPerSecond = 10.0;
    raw.channels       = { channel };

    const auto clip = BuildClip(raw);

    EXPECT_EQ(clip.name, "Run");
    ASSERT_EQ(clip.ChannelCount(), 1u);
    EXPECT_EQ(clip.channels[0].boneName, "Spine");
    EXPECT_EQ(clip.channels[0].posValues[0], glm::vec3(1.0f, 2.0f, 3.0f));
    EXPECT_EQ(clip.channels[0].posValues[1], glm::vec3(4.0f, 5.0f, 6.0f));
}

TEST(t_ClipBuild, AllThreeTracksAreConverted)
{
    RawChannel channel;
    channel.boneName    = "Root";
    channel.posTimes    = { 20.0 };  channel.posValues.assign(1, glm::vec3(0.0f));
    channel.rotTimes    = { 40.0 };  channel.rotValues.assign(1, glm::quat(1, 0, 0, 0));
    channel.scaleTimes  = { 60.0 };  channel.scaleValues.assign(1, glm::vec3(1.0f));

    RawClip raw;
    raw.ticksPerSecond = 20.0;
    raw.channels       = { channel };

    const auto clip = BuildClip(raw);

    ASSERT_EQ(clip.ChannelCount(), 1u);
    EXPECT_NEAR(clip.channels[0].posTimes[0],   1.0f, kEps);
    EXPECT_NEAR(clip.channels[0].rotTimes[0],   2.0f, kEps);
    EXPECT_NEAR(clip.channels[0].scaleTimes[0], 3.0f, kEps);
}

TEST(t_ClipBuild, BuiltChannelsAreConsistent)
{
    RawClip raw;
    raw.ticksPerSecond = 24.0;
    raw.channels       = { PositionChannel("Root", { 0.0, 1.0, 2.0 }) };

    const auto clip = BuildClip(raw);

    ASSERT_EQ(clip.ChannelCount(), 1u);
    EXPECT_TRUE(clip.channels[0].IsConsistent());
}

// Dropped, not repaired: silently truncating hides whatever produced the
// mismatch, and a half-written channel samples as garbage.
TEST(t_ClipBuild, ChannelWithMismatchedTimesAndValuesIsDropped)
{
    RawChannel broken;
    broken.boneName  = "Broken";
    broken.posTimes  = { 0.0, 1.0, 2.0 };
    broken.posValues = { glm::vec3(0.0f) };          // one value, three times

    RawClip raw;
    raw.ticksPerSecond = 24.0;
    raw.channels       = { PositionChannel("Good", { 0.0 }), broken };

    size_t dropped = 0;
    const auto clip = BuildClip(raw, &dropped);

    EXPECT_EQ(dropped, 1u);
    ASSERT_EQ(clip.ChannelCount(), 1u);
    EXPECT_EQ(clip.channels[0].boneName, "Good");
}

TEST(t_ClipBuild, DroppedCountIsZeroForCleanInput)
{
    RawClip raw;
    raw.ticksPerSecond = 24.0;
    raw.channels       = { PositionChannel("A", { 0.0 }), PositionChannel("B", { 0.0 }) };

    size_t dropped = 99;
    const auto clip = BuildClip(raw, &dropped);

    EXPECT_EQ(dropped, 0u);
    EXPECT_EQ(clip.ChannelCount(), 2u);
}

TEST(t_ClipBuild, EmptyClipIsHandled)
{
    const RawClip raw;
    const auto clip = BuildClip(raw);

    EXPECT_EQ(clip.ChannelCount(), 0u);
    EXPECT_FLOAT_EQ(clip.duration, 0.0f);
    EXPECT_TRUE(std::isfinite(clip.duration));
}

TEST(t_ClipBuild, EmptyTracksAreLegalAndKept)
{
    RawChannel rotationOnly;
    rotationOnly.boneName = "Root";
    rotationOnly.rotTimes  = { 0.0, 24.0 };
    rotationOnly.rotValues = { glm::quat(1, 0, 0, 0), glm::quat(1, 0, 0, 0) };

    RawClip raw;
    raw.ticksPerSecond = 24.0;
    raw.channels       = { rotationOnly };

    size_t dropped = 0;
    const auto clip = BuildClip(raw, &dropped);

    EXPECT_EQ(dropped, 0u);
    ASSERT_EQ(clip.ChannelCount(), 1u);
    EXPECT_TRUE(clip.channels[0].posTimes.empty());
    EXPECT_EQ(clip.channels[0].rotTimes.size(), 2u);
}
