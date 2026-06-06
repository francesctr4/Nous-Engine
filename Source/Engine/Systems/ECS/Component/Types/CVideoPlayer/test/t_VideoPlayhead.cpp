#include <gtest/gtest.h>

#include "Engine/Systems/ECS/Component/Types/CVideoPlayer/include/VideoPlayhead.h"

TEST(t_VideoPlayhead, AdvancesByDt)
{
    EXPECT_DOUBLE_EQ(AdvanceVideoPlayhead(0.0, 0.5, 10.0, false), 0.5);
    EXPECT_DOUBLE_EQ(AdvanceVideoPlayhead(0.5, 0.25, 10.0, false), 0.75);
}

TEST(t_VideoPlayhead, NonLoopClampsAtEnd)
{
    EXPECT_DOUBLE_EQ(AdvanceVideoPlayhead(9.9, 1.0, 10.0, false), 10.0);
    EXPECT_DOUBLE_EQ(AdvanceVideoPlayhead(10.0, 1.0, 10.0, false), 10.0);
}

TEST(t_VideoPlayhead, LoopWrapsIntoRange)
{
    EXPECT_DOUBLE_EQ(AdvanceVideoPlayhead(9.5, 1.0, 10.0, true), 0.5);   // 10.5 -> 0.5
    EXPECT_DOUBLE_EQ(AdvanceVideoPlayhead(0.0, 10.0, 10.0, true), 0.0);  // exactly one period
}

TEST(t_VideoPlayhead, UnknownDurationJustAdvances)
{
    EXPECT_DOUBLE_EQ(AdvanceVideoPlayhead(5.0, 1.0, 0.0,  true),  6.0);
    EXPECT_DOUBLE_EQ(AdvanceVideoPlayhead(5.0, 1.0, -1.0, false), 6.0);
}

TEST(t_VideoPlayhead, NeverNegative)
{
    EXPECT_DOUBLE_EQ(AdvanceVideoPlayhead(0.0, -5.0, 10.0, false), 0.0);
}

TEST(t_VideoPlayhead, AudioClockActiveFollowsAudioSeconds)
{
    // When the audio clock is active, the video playhead snaps to the audio time,
    // ignoring its own current/dt entirely.
    EXPECT_DOUBLE_EQ(ResolveVideoPlayhead(0.0, 0.016, 10.0, false, true, 3.5), 3.5);
    EXPECT_DOUBLE_EQ(ResolveVideoPlayhead(9.0, 0.016, 10.0, true,  true, 0.25), 0.25);
}

TEST(t_VideoPlayhead, AudioClockInactiveFallsBackToDtClock)
{
    // No audio clock → behaves exactly like AdvanceVideoPlayhead(current, dt, ...).
    EXPECT_DOUBLE_EQ(ResolveVideoPlayhead(0.5, 0.25, 10.0, false, false, 99.0),
                     AdvanceVideoPlayhead(0.5, 0.25, 10.0, false));
    EXPECT_DOUBLE_EQ(ResolveVideoPlayhead(9.9, 1.0, 10.0, false, false, 0.0),
                     AdvanceVideoPlayhead(9.9, 1.0, 10.0, false));
}

TEST(t_VideoPlayhead, AudioClockNeverNegative)
{
    // A spurious negative audio reading clamps to 0 rather than seeking backward past start.
    EXPECT_DOUBLE_EQ(ResolveVideoPlayhead(5.0, 0.016, 10.0, false, true, -1.0), 0.0);
}
