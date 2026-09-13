#include <AnimationSystem/Palette.h>

#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

using namespace nous::engine::animation_system;

namespace
{
    constexpr float kEps = 1e-4f;

    void ExpectMatNear(const glm::mat4& actual, const glm::mat4& expected, float eps = kEps)
    {
        for (int c = 0; c < 4; ++c)
        {
            for (int r = 0; r < 4; ++r)
            {
                EXPECT_NEAR(actual[c][r], expected[c][r], eps) << "column " << c << " row " << r;
            }
        }
    }

    void ExpectVec3Near(const glm::vec3& a, const glm::vec3& b, float eps = kEps)
    {
        EXPECT_NEAR(a.x, b.x, eps);
        EXPECT_NEAR(a.y, b.y, eps);
        EXPECT_NEAR(a.z, b.z, eps);
    }

    // A three-bone chain up the Y axis: Root at origin, Mid at y=1, Tip at y=2.
    // Offsets are the inverse global bind, which is what an importer writes.
    SkeletonData MakeChain()
    {
        SkeletonData s;
        s.names   = { "Root", "Mid", "Tip" };
        s.parents = { -1, 0, 1 };

        Transform root;
        Transform mid;  mid.position = { 0.0f, 1.0f, 0.0f };
        Transform tip;  tip.position = { 0.0f, 1.0f, 0.0f };   // local to Mid
        s.bindLocals = { root, mid, tip };

        // Global bind = accumulated locals; offset = its inverse.
        const glm::mat4 rootGlobal = root.ToMatrix();
        const glm::mat4 midGlobal  = rootGlobal * mid.ToMatrix();
        const glm::mat4 tipGlobal  = midGlobal  * tip.ToMatrix();

        s.offsets = { glm::inverse(rootGlobal), glm::inverse(midGlobal), glm::inverse(tipGlobal) };
        s.RebuildLookup();

        return s;
    }

    Pose BindPoseOf(const SkeletonData& skeleton, uint32_t uid)
    {
        Pose pose;
        pose.skeleton = uid;
        pose.bones    = skeleton.bindLocals;
        return pose;
    }
}

// ---------------------------------------------------------------------------
// BuildGlobals
// ---------------------------------------------------------------------------

TEST(t_Palette, BuildGlobalsAccumulatesDownTheChain)
{
    const SkeletonData skeleton = MakeChain();
    const Pose pose = BindPoseOf(skeleton, 1);

    std::vector<glm::mat4> globals;
    ASSERT_TRUE(BuildGlobals(skeleton, pose, globals));
    ASSERT_EQ(globals.size(), 3u);

    ExpectVec3Near(glm::vec3(globals[0][3]), { 0.0f, 0.0f, 0.0f });
    ExpectVec3Near(glm::vec3(globals[1][3]), { 0.0f, 1.0f, 0.0f });
    ExpectVec3Near(glm::vec3(globals[2][3]), { 0.0f, 2.0f, 0.0f });
}

TEST(t_Palette, BuildGlobalsPropagatesParentRotationToChildren)
{
    const SkeletonData skeleton = MakeChain();
    Pose pose = BindPoseOf(skeleton, 1);

    // Rotate the root 90 degrees about Z: +Y becomes -X, so the chain lies along
    // the negative X axis.
    pose.bones[0].rotation = glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    std::vector<glm::mat4> globals;
    ASSERT_TRUE(BuildGlobals(skeleton, pose, globals));

    ExpectVec3Near(glm::vec3(globals[1][3]), { -1.0f, 0.0f, 0.0f });
    ExpectVec3Near(glm::vec3(globals[2][3]), { -2.0f, 0.0f, 0.0f });
}

TEST(t_Palette, BuildGlobalsRejectsMismatchedBoneCount)
{
    const SkeletonData skeleton = MakeChain();
    Pose pose;
    pose.bones.resize(2);

    std::vector<glm::mat4> globals;
    EXPECT_FALSE(BuildGlobals(skeleton, pose, globals));
}

// A skeleton whose parents are not topologically sorted breaks the single-forward-
// loop assumption. Catching it here beats shipping a rig that is almost right.
TEST(t_Palette, BuildGlobalsRejectsNonTopologicalSkeleton)
{
    SkeletonData bad = MakeChain();
    bad.parents = { 1, -1, 1 };   // bone 0's parent comes after it

    ASSERT_FALSE(bad.IsTopologicallySorted());

    std::vector<glm::mat4> globals;
    EXPECT_FALSE(BuildGlobals(bad, BindPoseOf(bad, 1), globals));
}

TEST(t_Palette, ChainSkeletonIsWellFormed)
{
    const SkeletonData skeleton = MakeChain();

    EXPECT_TRUE(skeleton.IsConsistent());
    EXPECT_TRUE(skeleton.IsTopologicallySorted());
    EXPECT_EQ(skeleton.FindBone("Mid"), 1);
    EXPECT_EQ(skeleton.FindBone("Nope"), -1);
}

// ---------------------------------------------------------------------------
// BuildPalette -- the spec calls this the most useful test in the suite
// ---------------------------------------------------------------------------

TEST(t_Palette, BindPoseProducesIdentityMatrices)
{
    const SkeletonData skeleton = MakeChain();
    const Pose bindPose = BindPoseOf(skeleton, 1);

    std::vector<glm::mat4> globals;
    std::vector<glm::mat4> palette;
    ASSERT_TRUE(BuildPalette(skeleton, bindPose, globals, palette));
    ASSERT_EQ(palette.size(), 3u);

    for (const glm::mat4& m : palette) ExpectMatNear(m, glm::mat4(1.0f));
}

TEST(t_Palette, AnimatedPoseMovesVerticesByTheBoneDelta)
{
    const SkeletonData skeleton = MakeChain();
    Pose pose = BindPoseOf(skeleton, 1);

    // Slide the whole rig 5 units along X. Every bone's palette entry should be
    // exactly that translation, since the pose is otherwise the bind pose.
    pose.bones[0].position = { 5.0f, 0.0f, 0.0f };

    std::vector<glm::mat4> globals;
    std::vector<glm::mat4> palette;
    ASSERT_TRUE(BuildPalette(skeleton, pose, globals, palette));

    const glm::mat4 expected = glm::translate(glm::mat4(1.0f), glm::vec3(5.0f, 0.0f, 0.0f));
    for (const glm::mat4& m : palette) ExpectMatNear(m, expected);
}

TEST(t_Palette, RootGlobalInverseIsAppliedToEveryBone)
{
    const SkeletonData skeleton = MakeChain();
    const Pose bindPose = BindPoseOf(skeleton, 1);

    std::vector<glm::mat4> globals;
    ASSERT_TRUE(BuildGlobals(skeleton, bindPose, globals));

    const glm::mat4 rootInverse = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -3.0f));

    std::vector<glm::mat4> palette;
    ASSERT_TRUE(BuildPalette(skeleton, globals, palette, rootInverse));

    for (const glm::mat4& m : palette) ExpectMatNear(m, rootInverse);
}

TEST(t_Palette, BuildPaletteRejectsWrongGlobalsCount)
{
    const SkeletonData skeleton = MakeChain();
    const std::vector<glm::mat4> globals(2, glm::mat4(1.0f));

    std::vector<glm::mat4> palette;
    EXPECT_FALSE(BuildPalette(skeleton, globals, palette));
}

// ---------------------------------------------------------------------------
// SkinVertices
// ---------------------------------------------------------------------------

TEST(t_Palette, SkinningWithIdentityPaletteIsAPassthrough)
{
    const std::vector<glm::mat4>  palette(1, glm::mat4(1.0f));
    const std::vector<glm::vec3>  positions = { { 1.0f, 2.0f, 3.0f } };
    const std::vector<glm::vec3>  normals   = { { 0.0f, 1.0f, 0.0f } };
    const std::vector<glm::uvec4> ids       = { { 0u, 0u, 0u, 0u } };
    const std::vector<glm::vec4>  weights   = { { 1.0f, 0.0f, 0.0f, 0.0f } };

    std::vector<glm::vec3> outPositions(1);
    std::vector<glm::vec3> outNormals(1);

    ASSERT_TRUE(SkinVertices(palette, positions, normals, ids, weights, outPositions, outNormals));

    ExpectVec3Near(outPositions[0], positions[0]);
    ExpectVec3Near(outNormals[0],   normals[0]);
}

TEST(t_Palette, SkinningAppliesASingleBoneTransform)
{
    const std::vector<glm::mat4> palette =
        { glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 5.0f, 0.0f)) };

    const std::vector<glm::vec3>  positions = { { 1.0f, 0.0f, 0.0f } };
    const std::vector<glm::vec3>  normals   = { { 0.0f, 1.0f, 0.0f } };
    const std::vector<glm::uvec4> ids       = { { 0u, 0u, 0u, 0u } };
    const std::vector<glm::vec4>  weights   = { { 1.0f, 0.0f, 0.0f, 0.0f } };

    std::vector<glm::vec3> outPositions(1);
    std::vector<glm::vec3> outNormals(1);

    ASSERT_TRUE(SkinVertices(palette, positions, normals, ids, weights, outPositions, outNormals));

    ExpectVec3Near(outPositions[0], { 1.0f, 5.0f, 0.0f });
    ExpectVec3Near(outNormals[0],   { 0.0f, 1.0f, 0.0f });   // translation-only
}

TEST(t_Palette, SkinningBlendsTwoBonesByWeight)
{
    const std::vector<glm::mat4> palette = {
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f,  0.0f, 0.0f)),
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 10.0f, 0.0f)),
    };

    const std::vector<glm::vec3>  positions = { { 0.0f, 0.0f, 0.0f } };
    const std::vector<glm::vec3>  normals   = { { 0.0f, 1.0f, 0.0f } };
    const std::vector<glm::uvec4> ids       = { { 0u, 1u, 0u, 0u } };
    const std::vector<glm::vec4>  weights   = { { 0.25f, 0.75f, 0.0f, 0.0f } };

    std::vector<glm::vec3> outPositions(1);
    std::vector<glm::vec3> outNormals(1);

    ASSERT_TRUE(SkinVertices(palette, positions, normals, ids, weights, outPositions, outNormals));

    ExpectVec3Near(outPositions[0], { 0.0f, 7.5f, 0.0f });
}

// Rigid geometry inside a skinned mesh has all-zero bone data. Accumulating into a
// zero matrix would send those vertices to the origin.
TEST(t_Palette, UnweightedVerticesPassThroughUnchanged)
{
    const std::vector<glm::mat4> palette =
        { glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 99.0f, 0.0f)) };

    const std::vector<glm::vec3>  positions = { { 1.0f, 2.0f, 3.0f } };
    const std::vector<glm::vec3>  normals   = { { 1.0f, 0.0f, 0.0f } };
    const std::vector<glm::uvec4> ids       = { { 0u, 0u, 0u, 0u } };
    const std::vector<glm::vec4>  weights   = { { 0.0f, 0.0f, 0.0f, 0.0f } };

    std::vector<glm::vec3> outPositions(1);
    std::vector<glm::vec3> outNormals(1);

    ASSERT_TRUE(SkinVertices(palette, positions, normals, ids, weights, outPositions, outNormals));

    ExpectVec3Near(outPositions[0], positions[0]);
    ExpectVec3Near(outNormals[0],   normals[0]);
}

// The failure mode the shared BuildSkeleton exists to prevent. If it happens
// anyway, a limb that does not move beats reading past the palette.
TEST(t_Palette, OutOfRangeBoneIndexIsSkippedNotRead)
{
    const std::vector<glm::mat4> palette =
        { glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 4.0f, 0.0f)) };

    const std::vector<glm::vec3>  positions = { { 0.0f, 0.0f, 0.0f } };
    const std::vector<glm::vec3>  normals   = { { 0.0f, 1.0f, 0.0f } };
    const std::vector<glm::uvec4> ids       = { { 0u, 99u, 0u, 0u } };
    const std::vector<glm::vec4>  weights   = { { 1.0f, 1.0f, 0.0f, 0.0f } };

    std::vector<glm::vec3> outPositions(1);
    std::vector<glm::vec3> outNormals(1);

    ASSERT_TRUE(SkinVertices(palette, positions, normals, ids, weights, outPositions, outNormals));

    // Only bone 0 contributed.
    ExpectVec3Near(outPositions[0], { 0.0f, 4.0f, 0.0f });
}

TEST(t_Palette, SkinningRotatesAndRenormalizesNormals)
{
    const glm::mat4 rot = glm::mat4_cast(glm::angleAxis(glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f)));

    const std::vector<glm::mat4>  palette   = { rot };
    const std::vector<glm::vec3>  positions = { { 1.0f, 0.0f, 0.0f } };
    const std::vector<glm::vec3>  normals   = { { 1.0f, 0.0f, 0.0f } };
    const std::vector<glm::uvec4> ids       = { { 0u, 0u, 0u, 0u } };
    const std::vector<glm::vec4>  weights   = { { 1.0f, 0.0f, 0.0f, 0.0f } };

    std::vector<glm::vec3> outPositions(1);
    std::vector<glm::vec3> outNormals(1);

    ASSERT_TRUE(SkinVertices(palette, positions, normals, ids, weights, outPositions, outNormals));

    ExpectVec3Near(outPositions[0], { 0.0f, 1.0f, 0.0f });
    ExpectVec3Near(outNormals[0],   { 0.0f, 1.0f, 0.0f });
    EXPECT_NEAR(glm::length(outNormals[0]), 1.0f, kEps);
}

TEST(t_Palette, SkinningRejectsMismatchedSpanLengths)
{
    const std::vector<glm::mat4>  palette(1, glm::mat4(1.0f));
    const std::vector<glm::vec3>  positions = { { 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f } };
    const std::vector<glm::vec3>  normals   = { { 0.0f, 1.0f, 0.0f } };            // short
    const std::vector<glm::uvec4> ids       = { { 0u, 0u, 0u, 0u }, { 0u, 0u, 0u, 0u } };
    const std::vector<glm::vec4>  weights   = { { 1.0f, 0.0f, 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 0.0f } };

    std::vector<glm::vec3> outPositions(2);
    std::vector<glm::vec3> outNormals(2);

    EXPECT_FALSE(SkinVertices(palette, positions, normals, ids, weights, outPositions, outNormals));
}

TEST(t_Palette, SkinningRejectsUndersizedOutput)
{
    const std::vector<glm::mat4>  palette(1, glm::mat4(1.0f));
    const std::vector<glm::vec3>  positions = { { 0.0f, 0.0f, 0.0f } };
    const std::vector<glm::vec3>  normals   = { { 0.0f, 1.0f, 0.0f } };
    const std::vector<glm::uvec4> ids       = { { 0u, 0u, 0u, 0u } };
    const std::vector<glm::vec4>  weights   = { { 1.0f, 0.0f, 0.0f, 0.0f } };

    std::vector<glm::vec3> outPositions;   // empty
    std::vector<glm::vec3> outNormals(1);

    EXPECT_FALSE(SkinVertices(palette, positions, normals, ids, weights, outPositions, outNormals));
}
