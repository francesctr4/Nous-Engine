#include <gtest/gtest.h>

#include <EditorUI/AnimationTimelineLayout.h>

#include <vector>

using namespace nous::editor::timeline;

namespace
{
    // A 200-pixel-wide ruler starting at x = 50, over a 2-second clip.
    RulerLayout MakeRuler() { return RulerLayout{ 50.0f, 200.0f, 2.0f }; }
}

TEST(t_AnimationTimelineLayout, TimeAndXAreInverses)
{
    const RulerLayout ruler = MakeRuler();

    EXPECT_FLOAT_EQ(TimeToX(ruler, 0.0f), 50.0f);
    EXPECT_FLOAT_EQ(TimeToX(ruler, 2.0f), 250.0f);
    EXPECT_FLOAT_EQ(TimeToX(ruler, 1.0f), 150.0f);

    EXPECT_FLOAT_EQ(XToTime(ruler, 150.0f), 1.0f);
    EXPECT_FLOAT_EQ(XToTime(ruler, 50.0f),  0.0f);
}

// A zero-duration or zero-width ruler must not divide by zero -- a clip whose
// Library/ entry failed to load has duration 0 and the window still draws.
TEST(t_AnimationTimelineLayout, DegenerateRulersCollapseToTheStart)
{
    EXPECT_FLOAT_EQ(TimeToX(RulerLayout{ 50.0f, 200.0f, 0.0f }, 1.0f), 50.0f);
    EXPECT_FLOAT_EQ(XToTime(RulerLayout{ 50.0f, 0.0f, 2.0f }, 150.0f), 0.0f);
}

// Clamped to [0, duration): an event AT duration can never be reached by a looping
// clip, so the window must not let one be authored there.
TEST(t_AnimationTimelineLayout, ClampKeepsTimeStrictlyInsideTheClip)
{
    EXPECT_FLOAT_EQ(ClampTime(-1.0f, 2.0f), 0.0f);
    EXPECT_LT(ClampTime(5.0f, 2.0f), 2.0f);
    EXPECT_GT(ClampTime(5.0f, 2.0f), 1.99f);
    EXPECT_FLOAT_EQ(ClampTime(1.0f, 2.0f), 1.0f);
    EXPECT_FLOAT_EQ(ClampTime(1.0f, 0.0f), 0.0f);
}

TEST(t_AnimationTimelineLayout, SnapRoundsToTheNearestGridStep)
{
    EXPECT_FLOAT_EQ(SnapTime(0.51f, 30.0f), 15.0f / 30.0f);   // nearest 1/30th
    EXPECT_FLOAT_EQ(SnapTime(0.49f, 30.0f), 15.0f / 30.0f);
    EXPECT_FLOAT_EQ(SnapTime(0.333f, 60.0f), 20.0f / 60.0f);
}

// Snapping OFF is 0, not 1 -- a grid of 1 per second is a legitimate coarse setting.
TEST(t_AnimationTimelineLayout, SnapIsDisabledByANonPositiveGrid)
{
    EXPECT_FLOAT_EQ(SnapTime(0.517f, 0.0f),  0.517f);
    EXPECT_FLOAT_EQ(SnapTime(0.517f, -1.0f), 0.517f);
}

TEST(t_AnimationTimelineLayout, HitTestPicksTheNearestMarkerWithinTheGrabRadius)
{
    const RulerLayout ruler = MakeRuler();
    const std::vector<float> times = { 0.5f, 1.0f, 1.9f };   // x = 100, 150, 240

    EXPECT_EQ(HitTestMarker(ruler, times, 151.0f, 6.0f), 1);
    EXPECT_EQ(HitTestMarker(ruler, times, 98.0f,  6.0f), 0);
    EXPECT_EQ(HitTestMarker(ruler, times, 200.0f, 6.0f), -1);   // nothing near
    EXPECT_EQ(HitTestMarker(ruler, {},    150.0f, 6.0f), -1);
}

// Two markers within one grab radius: the NEARER one wins, so dragging the one you
// clicked is not a coin flip.
TEST(t_AnimationTimelineLayout, HitTestPrefersTheCloserOfTwoOverlappingMarkers)
{
    const RulerLayout ruler = MakeRuler();
    const std::vector<float> times = { 1.0f, 1.02f };   // x = 150, 152

    EXPECT_EQ(HitTestMarker(ruler, times, 152.5f, 6.0f), 1);
    EXPECT_EQ(HitTestMarker(ruler, times, 149.5f, 6.0f), 0);
}
