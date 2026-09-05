#include <gtest/gtest.h>

#include <Renderer/PackPalettes.h>

#include <cstdint>
#include <vector>

namespace
{
    // Sentinel non-null pointers — PackPalettes only checks these for null, never
    // dereferences them, so casting integers is safe in tests.
    ResourceMesh*     MeshPtr(uintptr_t id) { return reinterpret_cast<ResourceMesh*>(id); }
    ResourceMaterial* MatPtr (uintptr_t id) { return reinterpret_cast<ResourceMaterial*>(id); }

    std::vector<glm::mat4> MakePalette(size_t boneCount, float marker)
    {
        std::vector<glm::mat4> p(boneCount, glm::mat4(1.0f));
        if (!p.empty()) p[0][3][0] = marker;
        return p;
    }

    // PackPalettes takes an already-ordered pointer span, so tests build the
    // pointer array the same way its callers do.
    std::vector<const GeometryRenderData*> Ptrs(const std::vector<GeometryRenderData>& v)
    {
        std::vector<const GeometryRenderData*> out;
        out.reserve(v.size());
        for (const auto& g : v) out.push_back(&g);
        return out;
    }
}

TEST(PackPalettes, Unskinned_GetsTheSentinel)
{
    std::vector<GeometryRenderData> input(1);
    input[0].geometry = MeshPtr(1);
    input[0].material = MatPtr(10);

    const auto ptrs   = Ptrs(input);
    const auto packed = PackPalettes(ptrs, 0);

    ASSERT_EQ(packed.bases.size(), 1u);
    EXPECT_EQ(packed.bases[0], c_noSkinPalette);
    EXPECT_TRUE(packed.palettes.empty());
}

TEST(PackPalettes, MixedSkinnedAndStatic_BasesPointAtTheirOwnRun)
{
    const auto paletteA = MakePalette(3, 7.0f);
    const auto paletteB = MakePalette(3, 9.0f);

    std::vector<GeometryRenderData> input(3);
    input[0].geometry = MeshPtr(1); input[0].palette = &paletteA;
    input[1].geometry = MeshPtr(2);                                  // static
    input[2].geometry = MeshPtr(3); input[2].palette = &paletteB;

    const auto ptrs   = Ptrs(input);
    const auto packed = PackPalettes(ptrs, 0);

    ASSERT_EQ(packed.bases.size(),    3u);
    ASSERT_EQ(packed.palettes.size(), 6u);

    EXPECT_EQ(packed.bases[0], 0u);
    EXPECT_EQ(packed.bases[1], c_noSkinPalette);
    EXPECT_EQ(packed.bases[2], 3u);

    EXPECT_FLOAT_EQ(packed.palettes[packed.bases[0]][3][0], 7.0f);
    EXPECT_FLOAT_EQ(packed.palettes[packed.bases[2]][3][0], 9.0f);
}

TEST(PackPalettes, BasePaletteSlot_OffsetsEveryBase)
{
    const auto palette = MakePalette(2, 1.0f);
    std::vector<GeometryRenderData> input(1);
    input[0].geometry = MeshPtr(1);
    input[0].palette  = &palette;

    const auto ptrs   = Ptrs(input);
    const auto packed = PackPalettes(ptrs, c_maxSkinnedBones);

    ASSERT_EQ(packed.bases.size(), 1u);
    EXPECT_EQ(packed.bases[0], c_maxSkinnedBones);
}

TEST(PackPalettes, BoneOverflow_FallsBackToTheSentinel)
{
    const auto big   = MakePalette(c_maxSkinnedBones, 1.0f);
    const auto small = MakePalette(4, 2.0f);

    std::vector<GeometryRenderData> input(2);
    input[0].geometry = MeshPtr(1); input[0].palette = &big;
    input[1].geometry = MeshPtr(2); input[1].palette = &small;

    const auto ptrs   = Ptrs(input);
    const auto packed = PackPalettes(ptrs, 0);

    ASSERT_EQ(packed.bases.size(), 2u);
    EXPECT_EQ(packed.bases[0], 0u);
    EXPECT_EQ(packed.bases[1], c_noSkinPalette);
    EXPECT_LE(packed.palettes.size(), static_cast<size_t>(c_maxSkinnedBones));
}

// An empty palette vector is CAnimator's "not usable" signal — a cleared slot or a
// failed build — and must never produce a zero-length run a shader could index into.
TEST(PackPalettes, EmptyPalette_TreatedAsUnskinned)
{
    const std::vector<glm::mat4> empty;
    std::vector<GeometryRenderData> input(1);
    input[0].geometry = MeshPtr(1);
    input[0].palette  = &empty;

    const auto ptrs   = Ptrs(input);
    const auto packed = PackPalettes(ptrs, 0);

    ASSERT_EQ(packed.bases.size(), 1u);
    EXPECT_EQ(packed.bases[0], c_noSkinPalette);
    EXPECT_TRUE(packed.palettes.empty());
}

// A null entry must still occupy a base slot, or every base after it shifts by one
// and silently addresses the wrong character's palette.
TEST(PackPalettes, NullEntry_StillOccupiesABaseSlot)
{
    const auto palette = MakePalette(2, 5.0f);
    std::vector<GeometryRenderData> input(2);
    input[1].geometry = MeshPtr(1);
    input[1].palette  = &palette;

    std::vector<const GeometryRenderData*> ptrs{ nullptr, &input[1] };
    const auto packed = PackPalettes(ptrs, 0);

    ASSERT_EQ(packed.bases.size(), 2u);
    EXPECT_EQ(packed.bases[0], c_noSkinPalette);
    EXPECT_EQ(packed.bases[1], 0u);
}
