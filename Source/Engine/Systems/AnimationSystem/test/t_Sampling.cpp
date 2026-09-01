#include <AnimationSystem/Sampling.h>

#include <AnimationSystem/AnimClip.h>
#include <AnimationSystem/Binding.h>

#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace nous::engine::animation_system;

namespace
{
    constexpr float    kEps = 1e-5f;
    constexpr uint32_t kSkeletonUID = 7;
    constexpr uint32_t kClipUID     = 42;

    void ExpectVec3Near(const glm::vec3& a, const glm::vec3& b, float eps = kEps)
    {
        EXPECT_NEAR(a.x, b.x, eps);
        EXPECT_NEAR(a.y, b.y, eps);
        EXPECT_NEAR(a.z, b.z, eps);
    }

    // Root at the origin, child offset one unit up. Bind locals are distinct from
    // identity on purpose: a bug that leaves undriven bones default-constructed
    // then shows up as a wrong value rather than a coincidentally right one.
    SkeletonData MakeTwoBoneSkeleton()
    {
        SkeletonData s;
        s.names   = { "Root", "Child" };
        s.parents = { -1, 0 };
        s.offsets = { glm::mat4(1.0f), glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f)) };

        Transform rootBind;
        Transform childBind;
        childBind.position = { 0.0f, 1.0f, 0.0f };

        s.bindLocals = { rootBind, childBind };
        s.RebuildLookup();

        return s;
    }

    // Three position keys on "Root" at t = 0, 1, 2 seconds.
    AnimClipData MakeThreeKeyClip()
    {
        AnimChannel channel;
        channel.boneName  = "Root";
        channel.posTimes  = { 0.0f, 1.0f, 2.0f };
        channel.posValues = { { 0.0f, 0.0f, 0.0f }, { 10.0f, 0.0f, 0.0f }, { 10.0f, 20.0f, 0.0f } };

        AnimClipData clip;
        clip.name     = "Walk";
        clip.duration = 2.0f;
        clip.channels = { channel };

        return clip;
    }

    struct Rig
    {
        SkeletonData     skeleton;
        AnimClipData     clip;
        AnimationBinding binding;
        AnimInstance     instance;
        Pose             pose;
    };

    Rig MakeRig()
    {
        Rig rig;
        rig.skeleton = MakeTwoBoneSkeleton();
        rig.clip     = MakeThreeKeyClip();
        rig.binding  = CreateBinding(rig.clip, kClipUID, rig.skeleton, kSkeletonUID);
        rig.instance.SetClip(&rig.clip, kClipUID, &rig.binding);
        return rig;
    }
}

// ---------------------------------------------------------------------------
// FindKey
// ---------------------------------------------------------------------------

TEST(t_Sampling, FindKeyOnEmptyTimesIsSafe)
{
    const std::vector<float> times;
    uint32_t cursor = 3;   // stale on purpose

    const KeyLocation key = FindKey(times, 1.0f, cursor);

    EXPECT_EQ(key.index, 0u);
    EXPECT_FLOAT_EQ(key.factor, 0.0f);
    EXPECT_EQ(cursor, 0u);
}

TEST(t_Sampling, FindKeyHitsExactKeyWithZeroFactor)
{
    const std::vector<float> times = { 0.0f, 1.0f, 2.0f };
    uint32_t cursor = 0;

    const KeyLocation key = FindKey(times, 1.0f, cursor);

    EXPECT_EQ(key.index, 1u);
    EXPECT_FLOAT_EQ(key.factor, 0.0f);
}

TEST(t_Sampling, FindKeyReturnsMidpointFactor)
{
    const std::vector<float> times = { 0.0f, 1.0f, 2.0f };
    uint32_t cursor = 0;

    const KeyLocation key = FindKey(times, 1.25f, cursor);

    EXPECT_EQ(key.index, 1u);
    EXPECT_NEAR(key.factor, 0.25f, kEps);
}

TEST(t_Sampling, FindKeyClampsBeforeFirstAndAfterLast)
{
    const std::vector<float> times = { 1.0f, 2.0f };
    uint32_t cursor = 0;

    const KeyLocation before = FindKey(times, -5.0f, cursor);
    EXPECT_EQ(before.index, 0u);
    EXPECT_FLOAT_EQ(before.factor, 0.0f);

    const KeyLocation after = FindKey(times, 99.0f, cursor);
    EXPECT_EQ(after.index, 1u);          // last key
    EXPECT_FLOAT_EQ(after.factor, 0.0f);
}

TEST(t_Sampling, FindKeyAdvancesCursorForward)
{
    const std::vector<float> times = { 0.0f, 1.0f, 2.0f, 3.0f };
    uint32_t cursor = 0;

    (void)FindKey(times, 0.5f, cursor);  EXPECT_EQ(cursor, 0u);
    (void)FindKey(times, 1.5f, cursor);  EXPECT_EQ(cursor, 1u);
    (void)FindKey(times, 2.5f, cursor);  EXPECT_EQ(cursor, 2u);
}

// The self-healing property: a caller that seeks backwards without resetting the
// cursor must still get the right key, just at the cost of a rescan.
TEST(t_Sampling, FindKeyRecoversFromBackwardsSeekWithoutReset)
{
    const std::vector<float> times = { 0.0f, 1.0f, 2.0f, 3.0f };
    uint32_t cursor = 3;

    const KeyLocation key = FindKey(times, 0.5f, cursor);

    EXPECT_EQ(key.index, 0u);
    EXPECT_NEAR(key.factor, 0.5f, kEps);
    EXPECT_EQ(cursor, 0u);
}

// Duplicate timestamps are legal (a step key / hold). This is the division-by-zero
// case the span guard exists for.
TEST(t_Sampling, FindKeyHandlesDuplicateTimestamps)
{
    const std::vector<float> times = { 0.0f, 1.0f, 1.0f, 2.0f };
    uint32_t cursor = 0;

    const KeyLocation key = FindKey(times, 1.0f, cursor);

    EXPECT_FLOAT_EQ(key.factor, 0.0f);
    EXPECT_TRUE(std::isfinite(key.factor));
}

// ---------------------------------------------------------------------------
// Sample
// ---------------------------------------------------------------------------

TEST(t_Sampling, SampleStampsUIDAndSizesPose)
{
    Rig rig = MakeRig();

    Sample(rig.instance, rig.skeleton, kSkeletonUID, rig.pose);

    EXPECT_EQ(rig.pose.skeleton, kSkeletonUID);
    EXPECT_EQ(rig.pose.BoneCount(), 2u);
}

TEST(t_Sampling, SampleHitsExactKeys)
{
    Rig rig = MakeRig();

    rig.instance.Seek(0.0f);
    Sample(rig.instance, rig.skeleton, kSkeletonUID, rig.pose);
    ExpectVec3Near(rig.pose.bones[0].position, { 0.0f, 0.0f, 0.0f });

    rig.instance.Seek(1.0f);
    Sample(rig.instance, rig.skeleton, kSkeletonUID, rig.pose);
    ExpectVec3Near(rig.pose.bones[0].position, { 10.0f, 0.0f, 0.0f });

    rig.instance.Seek(2.0f);
    Sample(rig.instance, rig.skeleton, kSkeletonUID, rig.pose);
    ExpectVec3Near(rig.pose.bones[0].position, { 10.0f, 20.0f, 0.0f });
}

TEST(t_Sampling, SampleInterpolatesMidpoint)
{
    Rig rig = MakeRig();

    rig.instance.Seek(0.5f);
    Sample(rig.instance, rig.skeleton, kSkeletonUID, rig.pose);

    ExpectVec3Near(rig.pose.bones[0].position, { 5.0f, 0.0f, 0.0f });
}

TEST(t_Sampling, SampleClampsBeforeFirstAndAfterLastKey)
{
    Rig rig = MakeRig();

    rig.instance.Seek(-3.0f);
    Sample(rig.instance, rig.skeleton, kSkeletonUID, rig.pose);
    ExpectVec3Near(rig.pose.bones[0].position, { 0.0f, 0.0f, 0.0f });

    rig.instance.Seek(99.0f);
    Sample(rig.instance, rig.skeleton, kSkeletonUID, rig.pose);
    ExpectVec3Near(rig.pose.bones[0].position, { 10.0f, 20.0f, 0.0f });
}

// The reason SkeletonData carries bindLocals. "Child" has no channel in this clip;
// if Sample left it default-constructed the joint would snap to the origin, and
// the symptom would look like a skinning bug rather than a sampling one.
TEST(t_Sampling, UndrivenBonesKeepTheirBindPose)
{
    Rig rig = MakeRig();

    Sample(rig.instance, rig.skeleton, kSkeletonUID, rig.pose);

    ExpectVec3Near(rig.pose.bones[1].position, { 0.0f, 1.0f, 0.0f });
}

TEST(t_Sampling, ChannelNamingAnUnknownBoneIsIgnored)
{
    Rig rig = MakeRig();

    AnimChannel ghost;
    ghost.boneName  = "NotOnThisRig";
    ghost.posTimes  = { 0.0f };
    ghost.posValues = { { 999.0f, 999.0f, 999.0f } };
    rig.clip.channels.push_back(ghost);

    rig.binding = CreateBinding(rig.clip, kClipUID, rig.skeleton, kSkeletonUID);
    rig.instance.SetClip(&rig.clip, kClipUID, &rig.binding);

    EXPECT_EQ(rig.binding.channelToBone[1], -1);

    Sample(rig.instance, rig.skeleton, kSkeletonUID, rig.pose);

    ExpectVec3Near(rig.pose.bones[0].position, { 0.0f, 0.0f, 0.0f });
    ExpectVec3Near(rig.pose.bones[1].position, { 0.0f, 1.0f, 0.0f });
}

TEST(t_Sampling, SampleWithoutClipYieldsBindPose)
{
    const SkeletonData skeleton = MakeTwoBoneSkeleton();
    AnimInstance instance;
    Pose pose;

    Sample(instance, skeleton, kSkeletonUID, pose);

    EXPECT_EQ(pose.BoneCount(), 2u);
    ExpectVec3Near(pose.bones[1].position, { 0.0f, 1.0f, 0.0f });
}

// ---------------------------------------------------------------------------
// Advance
// ---------------------------------------------------------------------------

TEST(t_Sampling, AdvanceAccumulatesScaledBySpeed)
{
    Rig rig = MakeRig();
    rig.instance.speed = 2.0f;

    Advance(rig.instance, 0.25f);

    EXPECT_NEAR(rig.instance.time, 0.5f, kEps);
}

TEST(t_Sampling, LoopingWrapsAroundDuration)
{
    Rig rig = MakeRig();
    rig.instance.loop = true;

    Advance(rig.instance, 2.5f);   // duration is 2.0

    EXPECT_NEAR(rig.instance.time, 0.5f, kEps);
    EXPECT_FALSE(IsFinished(rig.instance));
}

TEST(t_Sampling, LoopWrapResetsTheCursor)
{
    Rig rig = MakeRig();
    rig.instance.loop = true;

    Advance(rig.instance, 1.9f);
    Sample(rig.instance, rig.skeleton, kSkeletonUID, rig.pose);
    ASSERT_GT(rig.instance.cursor[0].x, 0u);   // cursor walked forward

    Advance(rig.instance, 0.2f);               // wraps to 0.1

    EXPECT_EQ(rig.instance.cursor[0].x, 0u);

    // And the sample after the wrap is the start of the clip, not the end of it.
    Sample(rig.instance, rig.skeleton, kSkeletonUID, rig.pose);
    ExpectVec3Near(rig.pose.bones[0].position, { 1.0f, 0.0f, 0.0f });
}

TEST(t_Sampling, LoopingBackwardsWrapsToPositiveTime)
{
    Rig rig = MakeRig();
    rig.instance.loop  = true;
    rig.instance.speed = -1.0f;

    Advance(rig.instance, 0.5f);   // 0.0 - 0.5 -> should land at 1.5, not -0.5

    EXPECT_NEAR(rig.instance.time, 1.5f, kEps);
}

TEST(t_Sampling, NonLoopingClampsAndFinishes)
{
    Rig rig = MakeRig();
    rig.instance.loop = false;

    Advance(rig.instance, 5.0f);

    EXPECT_NEAR(rig.instance.time, 2.0f, kEps);
    EXPECT_TRUE(IsFinished(rig.instance));
}

// Duration 0 is the case where "time >= duration" and fmod both misbehave.
TEST(t_Sampling, ZeroDurationClipIsFinishedAndDoesNotDivideByZero)
{
    AnimClipData still;
    still.duration = 0.0f;

    AnimInstance instance;
    instance.SetClip(&still, kClipUID, nullptr);
    instance.loop = true;

    Advance(instance, 1.0f);

    EXPECT_TRUE(std::isfinite(instance.time));
    EXPECT_FLOAT_EQ(instance.time, 0.0f);

    instance.loop = false;
    EXPECT_TRUE(IsFinished(instance));
}

TEST(t_Sampling, InstanceWithNoClipIsFinished)
{
    const AnimInstance instance;
    EXPECT_TRUE(IsFinished(instance));
}
