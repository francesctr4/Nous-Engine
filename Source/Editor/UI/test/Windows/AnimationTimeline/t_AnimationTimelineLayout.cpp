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

// ---------------------------------------------------------------------------
// ChooseTickStep -- the labelled grid on the ruler.
//
// The ruler used to draw one unlabelled tick per snap step, which says nothing
// about WHERE you are: a 30/s grid over a 2 s clip is 60 identical marks. A
// labelled grid has to pick a step that is round enough to read and wide enough
// to fit its own text, which is what this decides.
// ---------------------------------------------------------------------------

// Steps come from the 1-2-5 ladder, so a label is always a number a person reads
// at a glance (0.5 s, 2 s, 20 s) and never 0.3333 s.
TEST(t_AnimationTimelineLayout, TickStepComesFromTheOneTwoFiveLadder)
{
    for (const float duration : { 0.4f, 2.0f, 7.5f, 33.0f, 240.0f })
    {
        const float step = ChooseTickStep(duration, 600.0f, 60.0f);
        ASSERT_GT(step, 0.0f);

        // Reduce to its mantissa: dividing out the power of ten must leave 1, 2 or 5.
        const float decade   = std::pow(10.0f, std::floor(std::log10(step)));
        const float mantissa = step / decade;

        EXPECT_TRUE(std::abs(mantissa - 1.0f) < 1e-3f ||
                    std::abs(mantissa - 2.0f) < 1e-3f ||
                    std::abs(mantissa - 5.0f) < 1e-3f)
            << "duration " << duration << " gave step " << step;
    }
}

// The whole point of the minimum: a label must not be drawn on top of its
// neighbour. The step's pixel width is what the caller cannot compute for itself.
TEST(t_AnimationTimelineLayout, TickStepIsNeverNarrowerThanTheMinimumSpacing)
{
    for (const float duration : { 0.4f, 2.0f, 7.5f, 33.0f, 240.0f })
        for (const float width : { 120.0f, 600.0f, 1800.0f })
        {
            const float step     = ChooseTickStep(duration, width, 60.0f);
            const float stepPx   = width * (step / duration);

            EXPECT_GE(stepPx, 60.0f)
                << "duration " << duration << " width " << width << " step " << step;
        }
}

// A wider ruler earns a FINER grid -- the step may only shrink as pixels are added,
// never grow. Without this the ruler could coarsen as the window is dragged wider.
TEST(t_AnimationTimelineLayout, AWiderRulerNeverGetsACoarserStep)
{
    const float narrow = ChooseTickStep(2.0f, 200.0f,  60.0f);
    const float wide   = ChooseTickStep(2.0f, 1200.0f, 60.0f);

    EXPECT_LE(wide, narrow);
}

// Same degenerate contract as the rest of this header: 0 means "draw no grid",
// which is what a clip whose Library/ entry failed to load produces.
TEST(t_AnimationTimelineLayout, ADegenerateRulerAsksForNoGrid)
{
    EXPECT_FLOAT_EQ(ChooseTickStep(0.0f,  600.0f, 60.0f), 0.0f);
    EXPECT_FLOAT_EQ(ChooseTickStep(-1.0f, 600.0f, 60.0f), 0.0f);
    EXPECT_FLOAT_EQ(ChooseTickStep(2.0f,  0.0f,   60.0f), 0.0f);
}
