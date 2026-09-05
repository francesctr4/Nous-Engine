#include <gtest/gtest.h>

#include <AnimationSystem/Bounds.h>
#include <AnimationSystem/Palette.h>

#include <glm/gtc/matrix_transform.hpp>

#include <limits>
#include <vector>

using nous::engine::animation_system::ComputeSkinnedBounds;
using nous::engine::animation_system::SkinVertices;

namespace
{
    constexpr float k_inf = std::numeric_limits<float>::max();

    // The three-bone rig every containment test below shares.
    std::vector<glm::mat4> Palette()
    {
        return {
            glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 5.0f, 0.0f)),
            glm::rotate(glm::mat4(1.0f), 1.2f, glm::vec3(0.0f, 0.0f, 1.0f)),
            glm::scale(glm::mat4(1.0f), glm::vec3(2.0f)),
        };
    }
}

TEST(ComputeSkinnedBounds, IdentityPalette_ReturnsTheUnionOfTheBoneBoxes)
{
    const std::vector<glm::vec3> boneMin{ glm::vec3(-1.0f, -2.0f, -3.0f) };
    const std::vector<glm::vec3> boneMax{ glm::vec3( 1.0f,  2.0f,  3.0f) };
    const std::vector<glm::mat4> palette{ glm::mat4(1.0f) };

    glm::vec3 outMin, outMax;
    ASSERT_TRUE(ComputeSkinnedBounds(boneMin, boneMax, palette, outMin, outMax));

    EXPECT_FLOAT_EQ(outMin.x, -1.0f);
    EXPECT_FLOAT_EQ(outMax.z,  3.0f);
}

TEST(ComputeSkinnedBounds, TranslatedBone_GrowsTheBox)
{
    const std::vector<glm::vec3> boneMin{ glm::vec3(-1.0f), glm::vec3(-1.0f) };
    const std::vector<glm::vec3> boneMax{ glm::vec3( 1.0f), glm::vec3( 1.0f) };
    const std::vector<glm::mat4> palette{
        glm::mat4(1.0f),
        glm::translate(glm::mat4(1.0f), glm::vec3(10.0f, 0.0f, 0.0f)),
    };

    glm::vec3 outMin, outMax;
    ASSERT_TRUE(ComputeSkinnedBounds(boneMin, boneMax, palette, outMin, outMax));

    EXPECT_FLOAT_EQ(outMin.x, -1.0f);
    EXPECT_FLOAT_EQ(outMax.x, 11.0f);   // 1 + 10
}

TEST(ComputeSkinnedBounds, EmptyPalette_Fails)
{
    const std::vector<glm::vec3> boneMin{ glm::vec3(-1.0f) };
    const std::vector<glm::vec3> boneMax{ glm::vec3( 1.0f) };

    glm::vec3 outMin, outMax;
    EXPECT_FALSE(ComputeSkinnedBounds(boneMin, boneMax, {}, outMin, outMax));
}

// A bone nothing is weighted to keeps an inverted box. Transforming it anyway would
// fold FLT_MAX corners into the union and swallow the entire scene.
TEST(ComputeSkinnedBounds, BoneWithNoVertices_IsSkipped)
{
    const std::vector<glm::vec3> boneMin{ glm::vec3(-1.0f), glm::vec3( k_inf) };
    const std::vector<glm::vec3> boneMax{ glm::vec3( 1.0f), glm::vec3(-k_inf) };
    const std::vector<glm::mat4> palette{
        glm::mat4(1.0f),
        glm::translate(glm::mat4(1.0f), glm::vec3(1000.0f, 0.0f, 0.0f)),
    };

    glm::vec3 outMin, outMax;
    ASSERT_TRUE(ComputeSkinnedBounds(boneMin, boneMax, palette, outMin, outMax));

    EXPECT_FLOAT_EQ(outMin.x, -1.0f);
    EXPECT_FLOAT_EQ(outMax.x,  1.0f);
}

TEST(ComputeSkinnedBounds, EveryBoneEmpty_Fails)
{
    const std::vector<glm::vec3> boneMin{ glm::vec3( k_inf) };
    const std::vector<glm::vec3> boneMax{ glm::vec3(-k_inf) };
    const std::vector<glm::mat4> palette{ glm::mat4(1.0f) };

    glm::vec3 outMin, outMax;
    EXPECT_FALSE(ComputeSkinnedBounds(boneMin, boneMax, palette, outMin, outMax));
}

// A mesh need not use every bone of its skeleton, so a palette longer than the box
// array is normal and the extra bones simply have nothing to contribute.
TEST(ComputeSkinnedBounds, PaletteLongerThanBoneBoxes_UsesTheOverlap)
{
    const std::vector<glm::vec3> boneMin{ glm::vec3(-1.0f) };
    const std::vector<glm::vec3> boneMax{ glm::vec3( 1.0f) };
    const std::vector<glm::mat4> palette{
        glm::mat4(1.0f),
        glm::translate(glm::mat4(1.0f), glm::vec3(500.0f, 0.0f, 0.0f)),
    };

    glm::vec3 outMin, outMax;
    ASSERT_TRUE(ComputeSkinnedBounds(boneMin, boneMax, palette, outMin, outMax));

    EXPECT_FLOAT_EQ(outMax.x, 1.0f);
}

// THE test. The bound is cheap and deliberately loose; what makes it usable for
// culling is that it NEVER clips. LBS is a weighted average whose weights sum to 1,
// so a skinned vertex lies in the convex hull of its per-bone transformed positions,
// and a vertex only has weight on bones whose bind box already contains it. This
// checks that argument against the exact reference path rather than trusting it.
TEST(ComputeSkinnedBounds, ContainsEverythingSkinVerticesProduces)
{
    const std::vector<glm::mat4> palette = Palette();

    const std::vector<glm::vec3> positions{
        {-1.0f, -1.0f, -1.0f}, { 1.0f, -1.0f, -1.0f},
        {-1.0f,  1.0f,  1.0f}, { 1.0f,  1.0f,  1.0f},
        { 0.5f, -0.25f, 0.75f},
    };
    const std::vector<glm::vec3>  normals(positions.size(), glm::vec3(0.0f, 1.0f, 0.0f));
    const std::vector<glm::uvec4> ids{
        {0,1,2,0}, {1,2,0,0}, {2,0,1,0}, {0,2,1,0}, {1,0,2,0},
    };
    const std::vector<glm::vec4> weights{
        {0.5f, 0.3f, 0.2f, 0.0f}, {0.2f, 0.5f, 0.3f, 0.0f},
        {0.34f, 0.33f, 0.33f, 0.0f}, {1.0f, 0.0f, 0.0f, 0.0f},
        {0.0f, 0.6f, 0.4f, 0.0f},
    };

    // Per-bone bind boxes, built exactly as ResourceMesh::RecomputeDerivedData does:
    // a vertex expands a bone's box only where that influence carries weight.
    std::vector<glm::vec3> boneMin(palette.size(), glm::vec3( k_inf));
    std::vector<glm::vec3> boneMax(palette.size(), glm::vec3(-k_inf));

    for (size_t v = 0; v < positions.size(); ++v)
    {
        for (int i = 0; i < 4; ++i)
        {
            if (weights[v][i] <= 0.0f) continue;
            const uint32_t b = ids[v][i];
            boneMin[b] = glm::min(boneMin[b], positions[v]);
            boneMax[b] = glm::max(boneMax[b], positions[v]);
        }
    }

    std::vector<glm::vec3> outPos(positions.size());
    std::vector<glm::vec3> outNrm(positions.size());
    ASSERT_TRUE(SkinVertices(palette, positions, normals, ids, weights, outPos, outNrm));

    glm::vec3 boxMin, boxMax;
    ASSERT_TRUE(ComputeSkinnedBounds(boneMin, boneMax, palette, boxMin, boxMax));

    for (const glm::vec3& p : outPos)
    {
        EXPECT_GE(p.x, boxMin.x - 1e-4f); EXPECT_LE(p.x, boxMax.x + 1e-4f);
        EXPECT_GE(p.y, boxMin.y - 1e-4f); EXPECT_LE(p.y, boxMax.y + 1e-4f);
        EXPECT_GE(p.z, boxMin.z - 1e-4f); EXPECT_LE(p.z, boxMax.z + 1e-4f);
    }
}

// The reason per-bone boxes exist. Feeding every bone the WHOLE mesh box also
// satisfies the containment proof, but each bone then drags the full extent to its
// own position and the union blows up -- which is what a character-sized debug box
// several times too large looks like in the viewport.
TEST(ComputeSkinnedBounds, PerBoneBoxes_AreTighterThanTheWholeMeshBox)
{
    const std::vector<glm::mat4> palette = Palette();

    // Each bone owns a small, disjoint region of a mesh spanning [-1, 1].
    const std::vector<glm::vec3> boneMin{
        {-1.0f, -1.0f, -1.0f}, {-0.2f, -0.2f, -0.2f}, { 0.8f, 0.8f, 0.8f},
    };
    const std::vector<glm::vec3> boneMax{
        {-0.8f, -0.8f, -0.8f}, { 0.2f,  0.2f,  0.2f}, { 1.0f, 1.0f, 1.0f},
    };

    // The rejected version: every bone carries the whole mesh extent.
    const std::vector<glm::vec3> wholeMin(palette.size(), glm::vec3(-1.0f));
    const std::vector<glm::vec3> wholeMax(palette.size(), glm::vec3( 1.0f));

    glm::vec3 tightMin, tightMax, looseMin, looseMax;
    ASSERT_TRUE(ComputeSkinnedBounds(boneMin,  boneMax,  palette, tightMin, tightMax));
    ASSERT_TRUE(ComputeSkinnedBounds(wholeMin, wholeMax, palette, looseMin, looseMax));

    const glm::vec3 tight = tightMax - tightMin;
    const glm::vec3 loose = looseMax - looseMin;

    EXPECT_LT(tight.x, loose.x);
    EXPECT_LT(tight.y, loose.y);
    EXPECT_LT(tight.z, loose.z);
}
