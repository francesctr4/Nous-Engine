#include <gtest/gtest.h>

#include "VideoFrameQueue/VideoFrameQueue.h"

#include <vector>

// ---------------- SelectNewestFrameIndex ----------------

TEST(t_VideoFrameQueue, SelectEmptyReturnsMinusOne)
{
    EXPECT_EQ(SelectNewestFrameIndex(nullptr, 0u, 1.0), -1);
}

TEST(t_VideoFrameQueue, SelectBeforeFirstReturnsMinusOne)
{
    const double pts[] = { 0.10, 0.20, 0.30 };
    EXPECT_EQ(SelectNewestFrameIndex(pts, 3u, 0.05), -1);
}

TEST(t_VideoFrameQueue, SelectPicksNewestNotAfterPlayhead)
{
    const double pts[] = { 0.00, 0.033, 0.066, 0.099 };
    EXPECT_EQ(SelectNewestFrameIndex(pts, 4u, 0.00),  0);
    EXPECT_EQ(SelectNewestFrameIndex(pts, 4u, 0.05),  1);   // 0.033 is newest <= 0.05
    EXPECT_EQ(SelectNewestFrameIndex(pts, 4u, 0.07),  2);
    EXPECT_EQ(SelectNewestFrameIndex(pts, 4u, 99.0),  3);   // clamps to last
}

// ---------------- VideoFrameQueue ----------------

static std::vector<uint8_t> SolidFrame(uint32 w, uint32 h, uint8_t v)
{
    return std::vector<uint8_t>(static_cast<size_t>(w) * h * 4, v);
}

TEST(t_VideoFrameQueue, TryPushReportsFullAtCapacity)
{
    VideoFrameQueue q(2);
    const auto f = SolidFrame(2, 2, 1);
    EXPECT_TRUE (q.TryPush(f.data(), 2, 2, 0.00));
    EXPECT_TRUE (q.TryPush(f.data(), 2, 2, 0.033));
    EXPECT_FALSE(q.TryPush(f.data(), 2, 2, 0.066));   // full
    EXPECT_EQ(q.Size(), 2u);
    EXPECT_EQ(q.Capacity(), 2u);
}

TEST(t_VideoFrameQueue, GetSelectsNewestAndDropsStaleAndFreesSlots)
{
    VideoFrameQueue q(4);
    q.TryPush(SolidFrame(2, 2, 10).data(), 2, 2, 0.00);
    q.TryPush(SolidFrame(2, 2, 20).data(), 2, 2, 0.033);
    q.TryPush(SolidFrame(2, 2, 30).data(), 2, 2, 0.066);

    VideoFrame out{};
    ASSERT_TRUE(q.TryGetForPlayhead(0.05, out));   // picks pts 0.033, drops 0.00
    EXPECT_DOUBLE_EQ(out.ptsSec, 0.033);
    EXPECT_EQ(out.width, 2u);
    EXPECT_EQ(out.height, 2u);
    ASSERT_NE(out.pixels, nullptr);
    EXPECT_EQ(out.pixels[0], 20);                  // the 0.033 frame's bytes
    EXPECT_EQ(q.Size(), 1u);                        // 0.00 and 0.033 consumed; 0.066 remains

    VideoFrame out2{};
    EXPECT_FALSE(q.TryGetForPlayhead(0.05, out2));  // nothing new <= 0.05
    ASSERT_TRUE (q.TryGetForPlayhead(0.10, out2));  // now 0.066 is deliverable
    EXPECT_DOUBLE_EQ(out2.ptsSec, 0.066);
    EXPECT_EQ(out2.pixels[0], 30);
    EXPECT_EQ(q.Size(), 0u);
}

TEST(t_VideoFrameQueue, GetBeforeAnyFrameReturnsFalse)
{
    VideoFrameQueue q(2);
    q.TryPush(SolidFrame(2, 2, 5).data(), 2, 2, 0.10);
    VideoFrame out{};
    EXPECT_FALSE(q.TryGetForPlayhead(0.05, out));   // first frame is in the future
    EXPECT_EQ(q.Size(), 1u);
}
