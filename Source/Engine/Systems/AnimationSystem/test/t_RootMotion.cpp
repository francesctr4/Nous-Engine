#include <gtest/gtest.h>

#include <AnimationSystem/Binding.h>
#include <AnimationSystem/Pose.h>
#include <AnimationSystem/RootMotion.h>
#include <AnimationSystem/Transform.h>

#include <glm/gtc/constants.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>

using nous::engine::animation_system::AnimationBinding;
using nous::engine::animation_system::BlendRootDelta;
using nous::engine::animation_system::ComputeRootDelta;
using nous::engine::animation_system::ExtractYaw;
using nous::engine::animation_system::Pose;
using nous::engine::animation_system::ResolveRootBone;
using nous::engine::animation_system::RootMotionDelta;
using nous::engine::animation_system::StripRootMotion;
using nous::engine::animation_system::Transform;

namespace
{
    Transform At(const float x, const float y, const float z)
    {
        Transform t;
        t.position = glm::vec3(x, y, z);
        return t;
    }

    Transform Yawed(const float radians)
    {
        Transform t;
        t.rotation = glm::angleAxis(radians, glm::vec3(0.0f, 1.0f, 0.0f));
        return t;
    }
}

// =============================================================================
// Root bone resolution
// =============================================================================

// Mixamo's bone 0 is a static RootNode with no channel; the travel lives on
// Hips. Taking bone 0 gives a constant zero delta and root motion appears
// broken with no error -- so the root is the LOWEST-INDEX bone the clip drives.
TEST(t_RootMotion, RootBoneIsTheLowestIndexDrivenBone)
{
    AnimationBinding binding;
    binding.channelToBone = { 5, 1, 9 };

    EXPECT_EQ(ResolveRootBone(binding), 1);
}

TEST(t_RootMotion, UnmatchedChannelsAreIgnoredWhenResolvingTheRoot)
{
    AnimationBinding binding;
    binding.channelToBone = { -1, 4, -1 };

    EXPECT_EQ(ResolveRootBone(binding), 4);
}

TEST(t_RootMotion, ResolveRootBoneReturnsMinusOneWhenNothingIsDriven)
{
    AnimationBinding binding;
    binding.channelToBone = { -1, -1 };

    EXPECT_EQ(ResolveRootBone(binding), -1);

    const AnimationBinding empty;
    EXPECT_EQ(ResolveRootBone(empty), -1);
}

// =============================================================================
// Delta extraction
// =============================================================================

// An in-place export has no travel on its root track, so every mode finds a zero
// delta and does nothing. This is why an in-place clip needs no special case.
TEST(t_RootMotion, AnInPlaceClipYieldsAZeroDelta)
{
    const Transform root = At(0.0f, 100.0f, 0.0f);

    const RootMotionDelta d = ComputeRootDelta(root, root, root, root, false);

    EXPECT_FLOAT_EQ(d.translation.x, 0.0f);
    EXPECT_FLOAT_EQ(d.translation.z, 0.0f);
    EXPECT_FLOAT_EQ(d.yaw, 0.0f);
}

TEST(t_RootMotion, HorizontalTravelBecomesTheDelta)
{
    const RootMotionDelta d = ComputeRootDelta(At(1.0f, 0.0f, 2.0f),
                                               At(4.0f, 0.0f, 6.0f),
                                               At(0.0f, 0.0f, 0.0f),
                                               At(0.0f, 0.0f, 0.0f),
                                               false);

    EXPECT_FLOAT_EQ(d.translation.x, 3.0f);
    EXPECT_FLOAT_EQ(d.translation.z, 4.0f);
}

// Vertical motion in a Mixamo clip is the hip bob of a gait. Hoisting it onto the
// GameObject makes the character bounce through whatever it stands on, and there
// is no collision to resolve against -- so Y stays in the pose.
TEST(t_RootMotion, VerticalMotionIsNotPartOfTheDelta)
{
    const RootMotionDelta d = ComputeRootDelta(At(0.0f, 100.0f, 0.0f),
                                               At(0.0f, 140.0f, 0.0f),
                                               At(0.0f, 0.0f, 0.0f),
                                               At(0.0f, 0.0f, 0.0f),
                                               false);

    EXPECT_FLOAT_EQ(d.translation.y, 0.0f);
}

TEST(t_RootMotion, YawBecomesPartOfTheDelta)
{
    const float quarter = glm::half_pi<float>();

    const RootMotionDelta d = ComputeRootDelta(Yawed(0.0f), Yawed(quarter),
                                               Transform{}, Transform{}, false);

    EXPECT_NEAR(d.yaw, quarter, 1e-4f);
}

// THE headline test. Delta is `now - previous`; at the loop seam the root snaps
// from the end of its travel back to the start, so the naive subtraction is one
// whole cycle BACKWARDS -- the character teleports back exactly as far as it just
// walked, every loop.
TEST(t_RootMotion, ALoopWrapYieldsOneFrameForwardNotOneCycleBack)
{
    const Transform clipStart = At(0.0f, 0.0f, 0.0f);
    const Transform clipEnd   = At(10.0f, 0.0f, 0.0f);

    // Was at 9.5 near the end; wrapped and is now 0.5 past the start. The real
    // motion is 0.5 to the end plus 0.5 from the start = 1.0 forward.
    const RootMotionDelta d = ComputeRootDelta(At(9.5f, 0.0f, 0.0f),
                                               At(0.5f, 0.0f, 0.0f),
                                               clipStart, clipEnd, true);

    EXPECT_FLOAT_EQ(d.translation.x, 1.0f);
}

// Crossing the +/-pi seam must not read as a near-full-circle spin.
TEST(t_RootMotion, YawDeltaTakesTheShortWayRoundTheSeam)
{
    const float justUnder = glm::pi<float>() - 0.1f;
    const float justOver  = -glm::pi<float>() + 0.1f;

    const RootMotionDelta d = ComputeRootDelta(Yawed(justUnder), Yawed(justOver),
                                               Transform{}, Transform{}, false);

    EXPECT_NEAR(d.yaw, 0.2f, 1e-3f);
}

// =============================================================================
// Stripping
// =============================================================================

TEST(t_RootMotion, StripMovesHorizontalToBindAndLeavesVertical)
{
    Pose pose;
    pose.bones.resize(2);
    pose.bones[0] = At(7.0f, 104.0f, -3.0f);

    Transform bind = At(0.0f, 100.0f, 0.0f);

    StripRootMotion(pose, 0, bind);

    EXPECT_FLOAT_EQ(pose.bones[0].position.x, 0.0f);
    EXPECT_FLOAT_EQ(pose.bones[0].position.z, 0.0f);
    EXPECT_FLOAT_EQ(pose.bones[0].position.y, 104.0f);   // the bob survives
}

TEST(t_RootMotion, StripRemovesYawButKeepsPitchAndRoll)
{
    const float pitch = 0.3f;

    Pose pose;
    pose.bones.resize(1);
    pose.bones[0].rotation = glm::angleAxis(0.8f, glm::vec3(0.0f, 1.0f, 0.0f))
                           * glm::angleAxis(pitch, glm::vec3(1.0f, 0.0f, 0.0f));

    StripRootMotion(pose, 0, Transform{});

    EXPECT_NEAR(ExtractYaw(pose.bones[0].rotation), 0.0f, 1e-4f);

    // The pitch is still there: the forward vector still tilts off horizontal.
    const glm::vec3 forward = pose.bones[0].rotation * glm::vec3(0.0f, 0.0f, 1.0f);
    EXPECT_GT(std::abs(forward.y), 0.1f);
}

// Out-of-range and "no root" must be no-ops rather than reads past the end: a
// clip that drives nothing resolves to -1 and still reaches this function.
TEST(t_RootMotion, StripIsANoOpForAnInvalidRootIndex)
{
    Pose pose;
    pose.bones.resize(1);
    pose.bones[0] = At(5.0f, 0.0f, 5.0f);

    StripRootMotion(pose, -1, Transform{});
    StripRootMotion(pose, 7, Transform{});

    EXPECT_FLOAT_EQ(pose.bones[0].position.x, 5.0f);
}

// =============================================================================
// Blending
// =============================================================================

// Deltas are blended, never positions. The blended root POSITION sweeps from
// clip A's root to clip B's as the fade weight moves 0->1, and that sweep is an
// artifact of blending rather than motion -- a positional delta cannot tell the
// difference, so it injects a lurch on every transition.
TEST(t_RootMotion, DeltasBlendByWeight)
{
    RootMotionDelta a; a.translation = glm::vec3(0.0f); a.yaw = 0.0f;
    RootMotionDelta b; b.translation = glm::vec3(4.0f, 0.0f, 0.0f); b.yaw = 1.0f;

    const RootMotionDelta mid = BlendRootDelta(a, b, 0.25f);

    EXPECT_FLOAT_EQ(mid.translation.x, 1.0f);
    EXPECT_FLOAT_EQ(mid.yaw, 0.25f);
}

TEST(t_RootMotion, BlendClampsWeightAndIsExactAtTheEnds)
{
    RootMotionDelta a; a.translation = glm::vec3(1.0f, 0.0f, 0.0f);
    RootMotionDelta b; b.translation = glm::vec3(9.0f, 0.0f, 0.0f);

    EXPECT_FLOAT_EQ(BlendRootDelta(a, b, 0.0f).translation.x, 1.0f);
    EXPECT_FLOAT_EQ(BlendRootDelta(a, b, 1.0f).translation.x, 9.0f);
    EXPECT_FLOAT_EQ(BlendRootDelta(a, b, -2.0f).translation.x, 1.0f);
    EXPECT_FLOAT_EQ(BlendRootDelta(a, b, 3.0f).translation.x, 9.0f);
}
