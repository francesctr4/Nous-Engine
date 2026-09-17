#include <gtest/gtest.h>

#include <RendererFrontend/GroupGeometries.h>

#include <cstdint>

// Sentinel non-null pointers — GroupGeometries only compares/checks-for-null these,
// never dereferences them, so casting integers is safe in tests.
static ResourceMesh*     MeshPtr(uintptr_t id)  { return reinterpret_cast<ResourceMesh*>(id); }
static ResourceMaterial* MatPtr(uintptr_t id)   { return reinterpret_cast<ResourceMaterial*>(id); }

static GeometryRenderData MakeGRD(ResourceMesh* mesh, ResourceMaterial* mat,
                                   const glm::mat4& model = glm::mat4(1.0f))
{
    GeometryRenderData g;
    g.geometry = mesh;
    g.material = mat;
    g.model    = model;
    return g;
}

// =============================================================================
// GroupGeometries — basic structural invariants
// =============================================================================

TEST(GroupGeometries, EmptyInput_ReturnsEmpty)
{
    const auto result = GroupGeometries({});
    EXPECT_TRUE(result.matrices.empty());
    EXPECT_TRUE(result.batches.empty());
}

TEST(GroupGeometries, SingleGeometry_OneBatchOneMatrix)
{
    const std::vector<GeometryRenderData> input = {
        MakeGRD(MeshPtr(1), MatPtr(10))
    };
    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.matrices.size(), 1u);
    ASSERT_EQ(result.batches.size(),  1u);
    EXPECT_EQ(result.batches[0].instanceCount, 1u);
    EXPECT_EQ(result.batches[0].geometry, MeshPtr(1));
    EXPECT_EQ(result.batches[0].material, MatPtr(10));
}

TEST(GroupGeometries, NullGeometry_Skipped)
{
    const std::vector<GeometryRenderData> input = {
        MakeGRD(nullptr,    MatPtr(10)),
        MakeGRD(MeshPtr(1), MatPtr(10)),
    };
    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.matrices.size(), 1u);
    ASSERT_EQ(result.batches.size(),  1u);
    EXPECT_EQ(result.batches[0].geometry, MeshPtr(1));
}

TEST(GroupGeometries, NullMaterial_Skipped)
{
    const std::vector<GeometryRenderData> input = {
        MakeGRD(MeshPtr(1), nullptr),
        MakeGRD(MeshPtr(1), MatPtr(10)),
    };
    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.matrices.size(), 1u);
    ASSERT_EQ(result.batches.size(),  1u);
    EXPECT_EQ(result.batches[0].material, MatPtr(10));
}

// =============================================================================
// GroupGeometries — batching logic
// =============================================================================

TEST(GroupGeometries, SameMeshAndMaterial_OneBatchTwoInstances)
{
    const std::vector<GeometryRenderData> input = {
        MakeGRD(MeshPtr(1), MatPtr(10)),
        MakeGRD(MeshPtr(1), MatPtr(10)),
    };
    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.matrices.size(), 2u);
    ASSERT_EQ(result.batches.size(),  1u);
    EXPECT_EQ(result.batches[0].instanceCount, 2u);
}

TEST(GroupGeometries, SameMaterialDifferentMesh_TwoBatches)
{
    const std::vector<GeometryRenderData> input = {
        MakeGRD(MeshPtr(1), MatPtr(10)),
        MakeGRD(MeshPtr(2), MatPtr(10)),
    };
    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.matrices.size(), 2u);
    ASSERT_EQ(result.batches.size(),  2u);
    EXPECT_EQ(result.batches[0].instanceCount, 1u);
    EXPECT_EQ(result.batches[1].instanceCount, 1u);
}

TEST(GroupGeometries, DifferentMaterials_TwoBatches)
{
    const std::vector<GeometryRenderData> input = {
        MakeGRD(MeshPtr(1), MatPtr(10)),
        MakeGRD(MeshPtr(1), MatPtr(20)),
    };
    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.matrices.size(), 2u);
    ASSERT_EQ(result.batches.size(),  2u);
}

TEST(GroupGeometries, InterleavedInput_SortedAndBatchedCorrectly)
{
    // Input order: mat10/mesh1, mat20/mesh1, mat10/mesh1 — sorting must merge the two mat10 entries.
    const std::vector<GeometryRenderData> input = {
        MakeGRD(MeshPtr(1), MatPtr(10)),
        MakeGRD(MeshPtr(1), MatPtr(20)),
        MakeGRD(MeshPtr(1), MatPtr(10)),
    };
    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.matrices.size(), 3u);
    ASSERT_EQ(result.batches.size(),  2u);  // mat10 batch (×2) + mat20 batch (×1)

    // Find the batch with instanceCount == 2 — it must be mat10.
    const auto* batch2 = (result.batches[0].instanceCount == 2)
                       ? &result.batches[0] : &result.batches[1];
    EXPECT_EQ(batch2->material, MatPtr(10));
    EXPECT_EQ(batch2->instanceCount, 2u);
}

// =============================================================================
// GroupGeometries — SSBO index / baseInstance
// =============================================================================

TEST(GroupGeometries, DefaultBaseInstance_FirstInstanceIsZero)
{
    const std::vector<GeometryRenderData> input = {
        MakeGRD(MeshPtr(1), MatPtr(10)),
    };
    const auto result = GroupGeometries(input);  // baseInstance defaults to 0

    ASSERT_EQ(result.batches.size(), 1u);
    EXPECT_EQ(result.batches[0].firstInstance, 0u);
}

TEST(GroupGeometries, BaseInstanceOffset_AppliedToFirstInstance)
{
    const std::vector<GeometryRenderData> input = {
        MakeGRD(MeshPtr(1), MatPtr(10)),
        MakeGRD(MeshPtr(2), MatPtr(10)),
    };
    const auto result = GroupGeometries(input, c_maxInstances);

    // Both batches use the game-pass SSBO range starting at c_maxInstances.
    ASSERT_EQ(result.batches.size(), 2u);
    EXPECT_EQ(result.batches[0].firstInstance, c_maxInstances + 0u);
    EXPECT_EQ(result.batches[1].firstInstance, c_maxInstances + 1u);
}

TEST(GroupGeometries, MatricesPreservedPerInstance)
{
    const glm::mat4 m1 = glm::mat4(2.0f);
    const glm::mat4 m2 = glm::mat4(3.0f);
    const std::vector<GeometryRenderData> input = {
        MakeGRD(MeshPtr(1), MatPtr(10), m1),
        MakeGRD(MeshPtr(1), MatPtr(10), m2),
    };
    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.matrices.size(), 2u);
    // Matrices are written in sorted order; both entries share the same key so their
    // relative order is preserved (stable_sort not guaranteed, but single-key groups
    // go through contiguous insertion).
    EXPECT_TRUE(result.matrices[0] == m1 || result.matrices[0] == m2);
    EXPECT_NE(result.matrices[0], result.matrices[1]);
}

// =============================================================================
// GroupGeometries — overflow cap
// =============================================================================

TEST(GroupGeometries, OverflowAtLimit_MatricesCappedAtMaxInstances)
{
    // Submit one more geometry than the per-pass cap.
    std::vector<GeometryRenderData> input;
    input.reserve(c_maxInstances + 1);
    for (uint32_t i = 0; i < c_maxInstances + 1; ++i)
        input.push_back(MakeGRD(MeshPtr(1), MatPtr(10)));

    const auto result = GroupGeometries(input);

    EXPECT_EQ(result.matrices.size(), c_maxInstances);
}

TEST(GroupGeometries, OverflowAtLimit_BatchInstanceCountReflectsCap)
{
    std::vector<GeometryRenderData> input;
    input.reserve(c_maxInstances + 10);
    for (uint32_t i = 0; i < c_maxInstances + 10; ++i)
        input.push_back(MakeGRD(MeshPtr(1), MatPtr(10)));

    const auto result = GroupGeometries(input);

    // All accepted instances belong to the same batch (same mesh+material).
    ASSERT_EQ(result.batches.size(), 1u);
    EXPECT_EQ(result.batches[0].instanceCount, c_maxInstances);
}

// =============================================================================
// Bone palettes
//
// The palette is indexed PER INSTANCE (bases[gl_InstanceIndex]) rather than
// selected per batch. That is the property the whole no-shader-variant approach
// rests on, and TwoPosesOfTheSameMesh_StillBatchTogether is what pins it.
// =============================================================================

static std::vector<glm::mat4> MakePalette(size_t boneCount, float marker)
{
    std::vector<glm::mat4> p(boneCount, glm::mat4(1.0f));
    if (!p.empty()) p[0][3][0] = marker;   // identifiable per-character value
    return p;
}

TEST(GroupGeometries, UnskinnedGeometry_GetsTheNoSkinSentinel)
{
    const std::vector<GeometryRenderData> input = {
        MakeGRD(MeshPtr(1), MatPtr(10))
    };
    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.paletteBases.size(), 1u);
    EXPECT_EQ(result.paletteBases[0], c_noSkinPalette);
    EXPECT_TRUE(result.palettes.empty());
}

// Bases must point at each character's own run inside the concatenated array, and
// unskinned instances in between must not consume palette slots.
TEST(GroupGeometries, MixedSkinnedAndStatic_BasesPointAtTheirOwnRun)
{
    const auto paletteA = MakePalette(3, 7.0f);
    const auto paletteB = MakePalette(3, 9.0f);

    std::vector<GeometryRenderData> input = {
        MakeGRD(MeshPtr(1), MatPtr(10)),   // skinned A
        MakeGRD(MeshPtr(2), MatPtr(10)),   // static
        MakeGRD(MeshPtr(3), MatPtr(10)),   // skinned B
    };
    input[0].palette = &paletteA;
    input[2].palette = &paletteB;

    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.paletteBases.size(), 3u);
    ASSERT_EQ(result.palettes.size(),     6u);

    // Sorted by (material, mesh); all share a material, so mesh order 1,2,3 holds.
    EXPECT_EQ(result.paletteBases[0], 0u);
    EXPECT_EQ(result.paletteBases[1], c_noSkinPalette);
    EXPECT_EQ(result.paletteBases[2], 3u);

    EXPECT_FLOAT_EQ(result.palettes[result.paletteBases[0]][3][0], 7.0f);
    EXPECT_FLOAT_EQ(result.palettes[result.paletteBases[2]][3][0], 9.0f);
}

// THE property that makes the no-variant approach work: two characters sharing a
// mesh and material stay in ONE instanced draw despite different poses. A shader
// variant would have had to split them.
TEST(GroupGeometries, TwoPosesOfTheSameMesh_StillBatchTogether)
{
    const auto paletteA = MakePalette(2, 1.0f);
    const auto paletteB = MakePalette(2, 2.0f);

    std::vector<GeometryRenderData> input = {
        MakeGRD(MeshPtr(1), MatPtr(10)),
        MakeGRD(MeshPtr(1), MatPtr(10)),
    };
    input[0].palette = &paletteA;
    input[1].palette = &paletteB;

    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.batches.size(), 1u);
    EXPECT_EQ(result.batches[0].instanceCount, 2u);

    ASSERT_EQ(result.paletteBases.size(), 2u);
    EXPECT_NE(result.paletteBases[0], result.paletteBases[1]);
    EXPECT_NE(result.paletteBases[0], c_noSkinPalette);
    EXPECT_NE(result.paletteBases[1], c_noSkinPalette);
}

TEST(GroupGeometries, BasePaletteSlot_OffsetsEveryBase)
{
    const auto palette = MakePalette(2, 1.0f);
    std::vector<GeometryRenderData> input = { MakeGRD(MeshPtr(1), MatPtr(10)) };
    input[0].palette = &palette;

    const auto result = GroupGeometries(input, c_maxInstances, c_maxSkinnedBones);

    ASSERT_EQ(result.paletteBases.size(), 1u);
    EXPECT_EQ(result.paletteBases[0], c_maxSkinnedBones);
    EXPECT_EQ(result.batches[0].firstInstance, c_maxInstances);
}

// Overflow degrades to bind pose, never to a read past the palette buffer.
TEST(GroupGeometries, BoneOverflow_FallsBackToTheSentinel)
{
    const auto big   = MakePalette(c_maxSkinnedBones, 1.0f);
    const auto small = MakePalette(4, 2.0f);

    std::vector<GeometryRenderData> input = {
        MakeGRD(MeshPtr(1), MatPtr(10)),
        MakeGRD(MeshPtr(2), MatPtr(10)),
    };
    input[0].palette = &big;      // consumes the whole budget
    input[1].palette = &small;    // no room left

    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.paletteBases.size(), 2u);
    EXPECT_EQ(result.paletteBases[0], 0u);
    EXPECT_EQ(result.paletteBases[1], c_noSkinPalette);
    EXPECT_LE(result.palettes.size(), static_cast<size_t>(c_maxSkinnedBones));
}

// An empty palette vector is CAnimator's "not usable" signal (a slot cleared, or a
// failed build) and must never produce a zero-length run a shader could index into.
TEST(GroupGeometries, EmptyPalette_TreatedAsUnskinned)
{
    const std::vector<glm::mat4> empty;
    std::vector<GeometryRenderData> input = { MakeGRD(MeshPtr(1), MatPtr(10)) };
    input[0].palette = &empty;

    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.paletteBases.size(), 1u);
    EXPECT_EQ(result.paletteBases[0], c_noSkinPalette);
    EXPECT_TRUE(result.palettes.empty());
}

// =============================================================================
// hasSkinnedInstances — drives the "this shader can't skin" warning
// =============================================================================

TEST(GroupGeometries, AllStaticInstances_BatchIsNotSkinned)
{
    const std::vector<GeometryRenderData> input = {
        MakeGRD(MeshPtr(1), MatPtr(10)),
        MakeGRD(MeshPtr(1), MatPtr(10)),
    };
    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.batches.size(), 1u);
    EXPECT_FALSE(result.batches[0].hasSkinnedInstances);
}

TEST(GroupGeometries, SkinnedInstance_BatchIsSkinned)
{
    const std::vector<glm::mat4> palette(3, glm::mat4(1.0f));

    std::vector<GeometryRenderData> input = { MakeGRD(MeshPtr(1), MatPtr(10)) };
    input[0].palette = &palette;

    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.batches.size(), 1u);
    EXPECT_TRUE(result.batches[0].hasSkinnedInstances);
}

// ANY, not ALL. Instancing deliberately collapses differently-posed characters into
// one batch, so a batch holding one skinned and one static instance still needs the
// palette bindings — reporting false here would suppress the warning for exactly the
// case that renders half the batch wrong.
TEST(GroupGeometries, MixedSkinnedAndStaticInSameBatch_BatchIsSkinned)
{
    const std::vector<glm::mat4> palette(3, glm::mat4(1.0f));

    std::vector<GeometryRenderData> input = {
        MakeGRD(MeshPtr(1), MatPtr(10)),
        MakeGRD(MeshPtr(1), MatPtr(10)),
    };
    input[1].palette = &palette;

    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.batches.size(), 1u);
    EXPECT_EQ(result.batches[0].instanceCount, 2u);
    EXPECT_TRUE(result.batches[0].hasSkinnedInstances);
}

// An empty palette is CAnimator's "no usable pose" signal and must not count as
// skinned, or a just-created animator makes every shader warn.
TEST(GroupGeometries, EmptyPalette_BatchIsNotSkinned)
{
    const std::vector<glm::mat4> empty;

    std::vector<GeometryRenderData> input = { MakeGRD(MeshPtr(1), MatPtr(10)) };
    input[0].palette = &empty;

    const auto result = GroupGeometries(input);

    ASSERT_EQ(result.batches.size(), 1u);
    EXPECT_FALSE(result.batches[0].hasSkinnedInstances);
}
