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
