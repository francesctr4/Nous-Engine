#include <AnimationSystem/Blending.h>

#include <gtest/gtest.h>

#include <glm/gtc/quaternion.hpp>

using namespace nous::engine::animation_system;

namespace
{
    constexpr float kEps = 1e-5f;

    Pose MakePose(uint32_t skeletonUID, std::initializer_list<glm::vec3> positions)
    {
        Pose pose;
        pose.skeleton = skeletonUID;

        for (const glm::vec3& p : positions)
        {
            Transform t;
            t.position = p;
            pose.bones.push_back(t);
        }

        return pose;
    }
}

TEST(t_Blending, WeightZeroReturnsAExactly)
{
    const Pose a = MakePose(1, { { 0.0f, 0.0f, 0.0f }, { 1.0f, 1.0f, 1.0f } });
    const Pose b = MakePose(1, { { 9.0f, 9.0f, 9.0f }, { 5.0f, 5.0f, 5.0f } });
    Pose out;

    ASSERT_TRUE(Blend(a, b, 0.0f, out));

    for (size_t i = 0; i < a.bones.size(); ++i)
    {
        EXPECT_EQ(out.bones[i].position, a.bones[i].position);
        EXPECT_EQ(out.bones[i].rotation, a.bones[i].rotation);
        EXPECT_EQ(out.bones[i].scale,    a.bones[i].scale);
    }
}

TEST(t_Blending, WeightOneReturnsBExactly)
{
    const Pose a = MakePose(1, { { 0.0f, 0.0f, 0.0f } });
    const Pose b = MakePose(1, { { 9.0f, 9.0f, 9.0f } });
    Pose out;

    ASSERT_TRUE(Blend(a, b, 1.0f, out));

    EXPECT_EQ(out.bones[0].position, b.bones[0].position);
    EXPECT_EQ(out.bones[0].rotation, b.bones[0].rotation);
    EXPECT_EQ(out.bones[0].scale,    b.bones[0].scale);
}

TEST(t_Blending, MidpointBlendsEveryBone)
{
    const Pose a = MakePose(1, { { 0.0f, 0.0f, 0.0f }, { 2.0f, 0.0f, 0.0f } });
    const Pose b = MakePose(1, { { 10.0f, 0.0f, 0.0f }, { 4.0f, 8.0f, 0.0f } });
    Pose out;

    ASSERT_TRUE(Blend(a, b, 0.5f, out));

    EXPECT_NEAR(out.bones[0].position.x, 5.0f, kEps);
    EXPECT_NEAR(out.bones[1].position.x, 3.0f, kEps);
    EXPECT_NEAR(out.bones[1].position.y, 4.0f, kEps);
}

TEST(t_Blending, OutputCarriesTheSkeletonUID)
{
    const Pose a = MakePose(77, { { 0.0f, 0.0f, 0.0f } });
    const Pose b = MakePose(77, { { 1.0f, 0.0f, 0.0f } });
    Pose out;

    ASSERT_TRUE(Blend(a, b, 0.5f, out));
    EXPECT_EQ(out.skeleton, 77u);
}

TEST(t_Blending, OutputPoseIsResizedFromEmpty)
{
    const Pose a = MakePose(1, { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } });
    const Pose b = MakePose(1, { { 2.0f, 0.0f, 0.0f }, { 3.0f, 0.0f, 0.0f } });
    Pose out;

    ASSERT_TRUE(Blend(a, b, 0.5f, out));
    EXPECT_EQ(out.BoneCount(), 2u);
}

// Bone index 7 is a different joint on a different rig, so a cross-skeleton blend
// produces confident garbage. The spec asks for an assert; this library cannot
// assert without taking a Logger dependency, so it refuses and says so.
TEST(t_Blending, MismatchedSkeletonUIDsAreRefused)
{
    const Pose a = MakePose(1, { { 0.0f, 0.0f, 0.0f } });
    const Pose b = MakePose(2, { { 1.0f, 0.0f, 0.0f } });
    Pose out;

    EXPECT_FALSE(ArePosesCompatible(a, b));
    EXPECT_FALSE(Blend(a, b, 0.5f, out));
}

TEST(t_Blending, MismatchedBoneCountsAreRefused)
{
    const Pose a = MakePose(1, { { 0.0f, 0.0f, 0.0f } });
    const Pose b = MakePose(1, { { 1.0f, 0.0f, 0.0f }, { 2.0f, 0.0f, 0.0f } });
    Pose out;

    EXPECT_FALSE(Blend(a, b, 0.5f, out));
}

TEST(t_Blending, RefusedBlendLeavesOutputUntouched)
{
    const Pose a = MakePose(1, { { 0.0f, 0.0f, 0.0f } });
    const Pose b = MakePose(2, { { 1.0f, 0.0f, 0.0f } });

    Pose out = MakePose(99, { { 4.0f, 4.0f, 4.0f } });

    EXPECT_FALSE(Blend(a, b, 0.5f, out));

    EXPECT_EQ(out.skeleton, 99u);
    EXPECT_EQ(out.bones[0].position, glm::vec3(4.0f, 4.0f, 4.0f));
}

TEST(t_Blending, BlendsRotationsAlongTheShortArc)
{
    const glm::vec3 axis(0.0f, 1.0f, 0.0f);

    Pose a; a.skeleton = 1; a.bones.resize(1);
    Pose b; b.skeleton = 1; b.bones.resize(1);
    a.bones[0].rotation = glm::angleAxis(glm::radians(0.0f),  axis);
    b.bones[0].rotation = glm::angleAxis(glm::radians(90.0f), axis);

    Pose out;
    ASSERT_TRUE(Blend(a, b, 0.5f, out));

    const glm::quat expected = glm::angleAxis(glm::radians(45.0f), axis);
    EXPECT_NEAR(std::abs(glm::dot(out.bones[0].rotation, expected)), 1.0f, kEps);
}

TEST(t_Blending, EmptyPosesBlendToAnEmptyPose)
{
    Pose a; a.skeleton = 1;
    Pose b; b.skeleton = 1;
    Pose out;

    EXPECT_TRUE(Blend(a, b, 0.5f, out));
    EXPECT_EQ(out.BoneCount(), 0u);
}
