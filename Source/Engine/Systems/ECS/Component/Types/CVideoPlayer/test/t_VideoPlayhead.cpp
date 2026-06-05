#include <gtest/gtest.h>

#include "Engine/Systems/ECS/Component/Types/CVideoPlayer/VideoPlayhead.h"

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
