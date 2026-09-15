#include <gtest/gtest.h>

#include <AnimationSystem/Events/AnimationEvents.h>

#include <algorithm>
#include <vector>

using nous::engine::animation_system::AnimationEvent;
using nous::engine::animation_system::CollectFiredEvents;
using nous::engine::animation_system::SortEvents;

namespace
{
    // Three events at 0.0, 0.5 and 1.5 in a 2-second clip. The one at 0.0 is what
    // makes rule 1's closed start observable; 1.5 sits near the seam.
    std::vector<AnimationEvent> MakeEvents()
    {
        std::vector<AnimationEvent> e(3);
        e[0].time = 0.0f;  e[0].name = "A";
        e[1].time = 0.5f;  e[1].name = "B";
        e[2].time = 1.5f;  e[2].name = "C";
        return e;
    }

    std::vector<int> Fired(const std::vector<AnimationEvent>& events,
                           float before, float after, float duration,
                           bool wrapped = false, bool reversed = false, bool finished = false)
    {
        std::vector<int> out;
        CollectFiredEvents(events, before, after, duration, wrapped, reversed, finished, out);
        return out;
    }
}

// Rule 1: closed at the start, so an event authored at t = 0 is reachable at all.
TEST(t_AnimationEvents, AnEventAtTimeZeroFiresOnTheFirstFrame)
{
    EXPECT_EQ(Fired(MakeEvents(), 0.0f, 0.1f, 2.0f), std::vector<int>({ 0 }));
}

// Rule 1: open at the end, so the boundary belongs to exactly one of two frames.
TEST(t_AnimationEvents, AnEventOnAFrameBoundaryFiresExactlyOnce)
{
    EXPECT_EQ(Fired(MakeEvents(), 0.0f, 0.5f, 2.0f), std::vector<int>({ 0 }));
    EXPECT_EQ(Fired(MakeEvents(), 0.5f, 1.0f, 2.0f), std::vector<int>({ 1 }));
}

TEST(t_AnimationEvents, EmissionFollowsTimeOrder)
{
    EXPECT_EQ(Fired(MakeEvents(), 0.0f, 2.0f, 2.0f), std::vector<int>({ 0, 1, 2 }));
}

// Rule 2: Advance clamps a non-looping clip at duration, so without the closed end
// an event authored there could never fire.
TEST(t_AnimationEvents, ANonLoopingFinalFrameFiresAnEventAtTheVeryEnd)
{
    std::vector<AnimationEvent> events(1);
    events[0].time = 2.0f;

    EXPECT_TRUE(Fired(events, 1.9f, 2.0f, 2.0f, false, false, /*finished*/ false).empty());
    EXPECT_EQ(Fired(events, 1.9f, 2.0f, 2.0f, false, false, /*finished*/ true),
              std::vector<int>({ 0 }));
}

// Rule 2's exception, and the bug it avoids: once clamped, before == after forever.
TEST(t_AnimationEvents, AFinishedClipDoesNotRefireEveryFrame)
{
    std::vector<AnimationEvent> events(1);
    events[0].time = 2.0f;

    EXPECT_TRUE(Fired(events, 2.0f, 2.0f, 2.0f, false, false, /*finished*/ true).empty());
}

// Rule 3: the seam. Both sides of the split fire, each exactly once.
TEST(t_AnimationEvents, AForwardWrapFiresBothSidesOfTheSeamOnce)
{
    // 1.9 -> wraps -> 0.2 : nothing above 1.9 on the near side, so only A (0.0) on
    // the far side.
    EXPECT_EQ(Fired(MakeEvents(), 1.9f, 0.2f, 2.0f, /*wrapped*/ true),
              std::vector<int>({ 0 }));

    // 1.4 -> wraps -> 0.6 : C on the near side, then A and B on the far side.
    EXPECT_EQ(Fired(MakeEvents(), 1.4f, 0.6f, 2.0f, /*wrapped*/ true),
              std::vector<int>({ 2, 0, 1 }));
}

// Rule 5: a frame covering more than a cycle must not replay it.
TEST(t_AnimationEvents, AFrameLongerThanTheClipFiresEachEventAtMostOnce)
{
    // 0.4 -> wraps -> 1.9, i.e. more than a full cycle traversed.
    const std::vector<int> fired = Fired(MakeEvents(), 0.4f, 1.9f, 2.0f, /*wrapped*/ true);

    for (int index : { 0, 1, 2 })
        EXPECT_EQ(std::count(fired.begin(), fired.end(), index), 1)
            << "event " << index << " fired more than once";
}

// Rule 4: descending, and closed at the side time starts from.
TEST(t_AnimationEvents, ReversePlaybackFiresDescending)
{
    EXPECT_EQ(Fired(MakeEvents(), 2.0f, 0.0f, 2.0f, false, /*reversed*/ true),
              std::vector<int>({ 2, 1 }));   // 0.0 is the open end
}

TEST(t_AnimationEvents, AReverseWrapSplitsTheOtherWay)
{
    // 0.2 -> wraps backwards -> 1.8 : A on the way down through 0, and nothing
    // between 1.8 and duration on the far side.
    EXPECT_EQ(Fired(MakeEvents(), 0.2f, 1.8f, 2.0f, /*wrapped*/ true, /*reversed*/ true),
              std::vector<int>({ 0 }));

    // 0.6 -> wraps backwards -> 1.4 : B and A down through 0, then C at 1.5 on the
    // far side coming back from duration.
    EXPECT_EQ(Fired(MakeEvents(), 0.6f, 1.4f, 2.0f, /*wrapped*/ true, /*reversed*/ true),
              std::vector<int>({ 1, 0, 2 }));
}

// A zero-width interval is how a STOPPED scene fires nothing -- Advance(0) traverses
// nothing, with no simulation-state query anywhere in the runtime.
TEST(t_AnimationEvents, AZeroWidthIntervalFiresNothing)
{
    EXPECT_TRUE(Fired(MakeEvents(), 0.5f, 0.5f, 2.0f).empty());
}

TEST(t_AnimationEvents, DegenerateInputsFireNothing)
{
    EXPECT_TRUE(Fired({}, 0.0f, 1.0f, 2.0f).empty());
    EXPECT_TRUE(Fired(MakeEvents(), 0.0f, 1.0f, /*duration*/ 0.0f).empty());
    EXPECT_TRUE(Fired(MakeEvents(), 0.0f, 1.0f, /*duration*/ -1.0f).empty());
}

// The collector never breaks early, so an unsorted stub still yields the right SET.
// Only emission order follows the vector -- which is why the writer sorts.
TEST(t_AnimationEvents, UnsortedInputStillYieldsEveryEventInTheInterval)
{
    std::vector<AnimationEvent> events(3);
    events[0].time = 1.5f;
    events[1].time = 0.0f;
    events[2].time = 0.5f;

    const std::vector<int> fired = Fired(events, 0.0f, 2.0f, 2.0f);
    EXPECT_EQ(fired.size(), 3u);
}

TEST(t_AnimationEvents, SortEventsIsStableAtEqualTimes)
{
    std::vector<AnimationEvent> events(3);
    events[0].time = 1.0f; events[0].name = "second";
    events[1].time = 0.0f; events[1].name = "first";
    events[2].time = 1.0f; events[2].name = "third";

    SortEvents(events);

    EXPECT_EQ(events[0].name, "first");
    EXPECT_EQ(events[1].name, "second");   // authoring order kept at equal times
    EXPECT_EQ(events[2].name, "third");
}
