#include <ResourceManager/Import/ModelParser/SkeletonBuild.h>

#include <gtest/gtest.h>

#include <glm/gtc/matrix_transform.hpp>

using namespace nous::engine::resource_manager;
using nous::engine::animation_system::SkeletonData;
using nous::engine::animation_system::Transform;

namespace
{
    constexpr float kEps = 1e-4f;

    RawBoneNode Node(std::string name, int parent, bool isBone,
                     glm::vec3 localPosition = glm::vec3(0.0f))
    {
        RawBoneNode node;
        node.name              = std::move(name);
        node.parent            = parent;
        node.isBone            = isBone;
        node.localBind.position = localPosition;
        return node;
    }

    void ExpectMatNear(const glm::mat4& actual, const glm::mat4& expected, float eps = kEps)
    {
        for (int c = 0; c < 4; ++c)
        {
            for (int r = 0; r < 4; ++r) EXPECT_NEAR(actual[c][r], expected[c][r], eps);
        }
    }
}

TEST(t_SkeletonBuild, EmptyInputYieldsEmptySkeleton)
{
    const auto result = BuildSkeleton({});

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->BoneCount(), 0u);
    EXPECT_TRUE(result->IsConsistent());
    EXPECT_TRUE(result->IsTopologicallySorted());
}

TEST(t_SkeletonBuild, SimpleChainIsKeptWhole)
{
    const std::vector<RawBoneNode> nodes = {
        Node("Root",  -1, true),
        Node("Spine",  0, true, { 0.0f, 1.0f, 0.0f }),
        Node("Head",   1, true, { 0.0f, 1.0f, 0.0f }),
    };

    const auto result = BuildSkeleton(nodes);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->BoneCount(), 3u);
    EXPECT_EQ(result->names, (std::vector<std::string>{ "Root", "Spine", "Head" }));
    EXPECT_EQ(result->parents, (std::vector<int>{ -1, 0, 1 }));
    EXPECT_TRUE(result->IsConsistent());
    EXPECT_TRUE(result->IsTopologicallySorted());
}

TEST(t_SkeletonBuild, LookupIsBuilt)
{
    const auto result = BuildSkeleton({ Node("Root", -1, true), Node("Spine", 0, true) });
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->FindBone("Root"),  0);
    EXPECT_EQ(result->FindBone("Spine"), 1);
    EXPECT_EQ(result->FindBone("Absent"), -1);
}

TEST(t_SkeletonBuild, BindLocalsSurviveUnchanged)
{
    const auto result = BuildSkeleton({
        Node("Root",  -1, true, { 1.0f, 2.0f, 3.0f }),
        Node("Spine",  0, true, { 0.0f, 4.0f, 0.0f }),
    });

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->bindLocals[0].position, glm::vec3(1.0f, 2.0f, 3.0f));
    EXPECT_EQ(result->bindLocals[1].position, glm::vec3(0.0f, 4.0f, 0.0f));
}

// The pruning rule, and the reason the pre-pass walks nodes rather than aiBones.
TEST(t_SkeletonBuild, NonBoneLeavesAreDropped)
{
    const std::vector<RawBoneNode> nodes = {
        Node("Root",     -1, true),
        Node("Spine",     0, true),
        Node("LightRig",  0, false),   // no bone below it -> dropped
        Node("Camera",    0, false),   // ditto
    };

    const auto result = BuildSkeleton(nodes);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->BoneCount(), 2u);
    EXPECT_EQ(result->FindBone("LightRig"), -1);
    EXPECT_EQ(result->FindBone("Camera"),   -1);
}

TEST(t_SkeletonBuild, WholeSubtreeWithNoBonesIsDropped)
{
    const std::vector<RawBoneNode> nodes = {
        Node("Root",      -1, true),
        Node("Geometry",   0, false),
        Node("Mesh_A",     1, false),
        Node("Mesh_B",     1, false),
        Node("Spine",      0, true),
    };

    const auto result = BuildSkeleton(nodes);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->BoneCount(), 2u);
    EXPECT_EQ(result->names, (std::vector<std::string>{ "Root", "Spine" }));
}

// The case that makes "ancestor of a bone" a rule rather than an optimization: a
// weightless helper joint between two real bones. Dropping it would put a hole in
// the parent chain, and the child would inherit the wrong transform.
TEST(t_SkeletonBuild, WeightlessIntermediateJointIsKept)
{
    const std::vector<RawBoneNode> nodes = {
        Node("Root",        -1, true),
        Node("HelperNull",   0, false),   // no weights, but Shoulder hangs off it
        Node("Shoulder",     1, true),
    };

    const auto result = BuildSkeleton(nodes);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->BoneCount(), 3u);
    EXPECT_EQ(result->FindBone("HelperNull"), 1);
    EXPECT_EQ(result->parents[2], 1);
}

// Pruning renumbers, and every surviving parent index has to be remapped with it.
// This is the off-by-one the whole pure split exists to make reachable by a test.
TEST(t_SkeletonBuild, ParentIndicesAreRemappedAfterPruning)
{
    const std::vector<RawBoneNode> nodes = {
        Node("Root",     -1, true),    // old 0 -> new 0
        Node("Junk_A",    0, false),   // dropped
        Node("Junk_B",    0, false),   // dropped
        Node("Spine",     0, true),    // old 3 -> new 1
        Node("Junk_C",    3, false),   // dropped
        Node("Head",      3, true),    // old 5 -> new 2, parent must become 1
    };

    const auto result = BuildSkeleton(nodes);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->names,   (std::vector<std::string>{ "Root", "Spine", "Head" }));
    EXPECT_EQ(result->parents, (std::vector<int>{ -1, 0, 1 }));
    EXPECT_TRUE(result->IsTopologicallySorted());
}

TEST(t_SkeletonBuild, OutputIsAlwaysTopologicallySortedAndConsistent)
{
    const std::vector<RawBoneNode> nodes = {
        Node("Root",   -1, true),
        Node("A",       0, false),
        Node("B",       1, true),
        Node("C",       0, true),
        Node("D",       2, true),
    };

    const auto result = BuildSkeleton(nodes);
    ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(result->IsTopologicallySorted());
    EXPECT_TRUE(result->IsConsistent());
}

TEST(t_SkeletonBuild, RealBonesKeepTheirAssimpOffset)
{
    RawBoneNode root = Node("Root", -1, true);
    root.offset = glm::translate(glm::mat4(1.0f), glm::vec3(7.0f, 0.0f, 0.0f));

    const auto result = BuildSkeleton({ root });
    ASSERT_TRUE(result.has_value());

    ExpectMatNear(result->offsets[0], root.offset);
}

// A kept non-bone has no offset of its own. Leaving identity there would silently
// misplace anything that later referenced the joint, so it is derived from the
// accumulated bind instead.
TEST(t_SkeletonBuild, KeptNonBoneGetsInverseGlobalBindAsItsOffset)
{
    const std::vector<RawBoneNode> nodes = {
        Node("Root",       -1, true,  { 0.0f, 0.0f, 0.0f }),
        Node("HelperNull",  0, false, { 0.0f, 3.0f, 0.0f }),
        Node("Hand",        1, true,  { 0.0f, 1.0f, 0.0f }),
    };

    const auto result = BuildSkeleton(nodes);
    ASSERT_TRUE(result.has_value());

    // Helper's global bind is a translation of +3 in Y, so its offset is -3.
    const glm::mat4 expected = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -3.0f, 0.0f));
    ExpectMatNear(result->offsets[1], expected);
}

TEST(t_SkeletonBuild, DerivedOffsetCancelsTheBindPose)
{
    const std::vector<RawBoneNode> nodes = {
        Node("Root",    -1, false, { 1.0f, 0.0f, 0.0f }),
        Node("Helper",   0, false, { 0.0f, 2.0f, 0.0f }),
        Node("Bone",     1, true,  { 0.0f, 0.0f, 5.0f }),
    };

    const auto result = BuildSkeleton(nodes);
    ASSERT_TRUE(result.has_value());

    // For every DERIVED offset, globalBind * offset must be identity -- that is
    // what "offset" means, and it is the same property t_AnimationSystem_Palette
    // checks end to end.
    glm::mat4 rootGlobal   = result->bindLocals[0].ToMatrix();
    glm::mat4 helperGlobal = rootGlobal * result->bindLocals[1].ToMatrix();

    ExpectMatNear(rootGlobal   * result->offsets[0], glm::mat4(1.0f));
    ExpectMatNear(helperGlobal * result->offsets[1], glm::mat4(1.0f));
}

// The DFS-order contract is validated, not assumed. A traversal that emitted a
// child before its parent would otherwise produce a rig that looks almost right.
TEST(t_SkeletonBuild, ForwardParentReferenceIsRejected)
{
    const std::vector<RawBoneNode> nodes = {
        Node("Child",  1, true),   // parent comes after
        Node("Parent", -1, true),
    };

    const auto result = BuildSkeleton(nodes);

    ASSERT_FALSE(result.has_value());
    EXPECT_NE(result.error().find("Child"), std::string::npos);
}

TEST(t_SkeletonBuild, SelfParentIsRejected)
{
    const auto result = BuildSkeleton({ Node("Loop", 0, true) });

    EXPECT_FALSE(result.has_value());
}

TEST(t_SkeletonBuild, MultipleRootsAreAllowed)
{
    const std::vector<RawBoneNode> nodes = {
        Node("RootA", -1, true),
        Node("RootB", -1, true),
        Node("ChildOfB", 1, true),
    };

    const auto result = BuildSkeleton(nodes);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->parents, (std::vector<int>{ -1, -1, 1 }));
    EXPECT_TRUE(result->IsTopologicallySorted());
}

TEST(t_SkeletonBuild, NoBonesAtAllYieldsAnEmptySkeleton)
{
    const std::vector<RawBoneNode> nodes = {
        Node("Root",  -1, false),
        Node("Mesh",   0, false),
    };

    const auto result = BuildSkeleton(nodes);

    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->BoneCount(), 0u);
}

// ─── ApplyAnimatedFallback ────────────────────────────────────────────────────
//
// An anim-only FBX (Mixamo "without skin") has no aiMesh, therefore no aiBone and
// no offset matrices at all -- animation channels are the only signal about which
// nodes are joints. These cover that fallback and, just as importantly, the gate
// that stops it touching a rig which already has real bones.

namespace
{
    // Root -> A -> B, plus an unrelated sibling "Prop" under Root.
    std::vector<RawBoneNode> BonelessHierarchy()
    {
        return {
            Node("Root", -1, false),
            Node("A",     0, false, { 0.0f, 2.0f, 0.0f }),
            Node("B",     1, false),
            Node("Prop",  0, false),
        };
    }
}

TEST(t_SkeletonBuild, AnimatedFallbackMarksChannelNamedNodesWhenNoBonesExist)
{
    auto nodes = BonelessHierarchy();
    ApplyAnimatedFallback(nodes, { "A", "B" });

    EXPECT_FALSE(nodes[0].isAnimated);
    EXPECT_TRUE (nodes[1].isAnimated);
    EXPECT_TRUE (nodes[2].isAnimated);
    EXPECT_FALSE(nodes[3].isAnimated);
}

TEST(t_SkeletonBuild, AnimatedNodesAreKeptAlongWithTheirAncestors)
{
    auto nodes = BonelessHierarchy();
    ApplyAnimatedFallback(nodes, { "B" });

    const auto result = BuildSkeleton(nodes);
    ASSERT_TRUE(result.has_value());

    // Root and A survive as ancestors of B; Prop is pruned.
    EXPECT_EQ(result->names, (std::vector<std::string>{ "Root", "A", "B" }));
    EXPECT_EQ(result->parents, (std::vector<int>{ -1, 0, 1 }));
    EXPECT_TRUE(result->IsConsistent());
    EXPECT_TRUE(result->IsTopologicallySorted());
}

// THE ONE THAT PROTECTS EVERY SKINNED IMPORT THAT WORKS TODAY.
//
// Applied unconditionally, the fallback would promote an animated non-bone node --
// root motion on a geometry node, a prop parented to a hand, an exporter helper --
// into an extra "bone" with a derived, non-authoritative offset. That silently
// changes rigs that import correctly, and breaks the property that a skinned FBX
// and its anim-only sibling produce identical bone-name lists.
TEST(t_SkeletonBuild, AnimatedFallbackIsIgnoredEntirelyWhenAnyBoneExists)
{
    auto nodes = BonelessHierarchy();
    nodes[1].isBone = true;                     // "A" carries weights
    ApplyAnimatedFallback(nodes, { "Prop" });   // an animated NON-bone node

    EXPECT_FALSE(nodes[3].isAnimated);

    const auto result = BuildSkeleton(nodes);
    ASSERT_TRUE(result.has_value());

    EXPECT_EQ(result->names, (std::vector<std::string>{ "Root", "A" }));
}

TEST(t_SkeletonBuild, AnimatedNonBonesGetDerivedOffsets)
{
    auto nodes = BonelessHierarchy();
    ApplyAnimatedFallback(nodes, { "A" });

    const auto result = BuildSkeleton(nodes);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(result->BoneCount(), 2u);

    // No aiBone offset exists, so "A" gets inverse(globalBind) -- here just the
    // inverse of its own local bind, since Root is identity.
    ExpectMatNear(result->offsets[1], glm::inverse(nodes[1].localBind.ToMatrix()));
}
