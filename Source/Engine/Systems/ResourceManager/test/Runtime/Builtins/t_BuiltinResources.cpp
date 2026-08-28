// Covers BuiltinResources: the engine's fallback textures and default material.
//
// These five resources are substituted whenever an asset fails to load, so their
// pixel VALUES are not cosmetic -- each is a neutral identity for the slot it
// stands in for, and a wrong one silently changes the shading of every mesh that
// falls back to it:
//   white       (1,1,1,1)     identity for MULTIPLICATIVE slots (AO, roughness)
//   black       (0,0,0,1)     identity for ADDITIVE slots (emissive)
//   flat normal (128,128,255) decodes to (0,0,1); white here would tilt 45 degrees
//   default     magenta/blue checkerboard, deliberately conspicuous
//
// Two structural properties matter as much as the values: the reserved UIDs must
// be distinct (the descriptor lazy-write dedup in WriteInstanceSampler keys off
// GetUID, so two fallbacks sharing a UID would alias each other's descriptors),
// and the textures must precede the material in the returned upload order.

#include <gtest/gtest.h>

#include <ResourceManager/Runtime/BuiltinResources.h>
#include <ResourceManager/Core/ResourceBase.h>
#include <ResourceManager/Types/ResourceTexture/ResourceTexture.h>
#include <ResourceManager/Types/ResourceMaterial/ResourceMaterial.h>
#include <Renderer/IGPUResourceFactory.h>
#include <MemoryManager/MemoryManager.h>
#include <EngineCore/InvalidID.h>

#include <set>
#include <vector>

namespace
{
    // Records what was asked of the GPU. Destroy() must only reach the GPU for
    // resources that actually made it to GPU_READY.
    class RecordingGpuFactory final : public IGPUResourceFactory
    {
    public:
        bool CreateTexture(const uint8_t*, ResourceTexture*) override { return true; }
        void DestroyTexture(ResourceTexture* t) override { destroyedTextures.push_back(t); }

        bool CreateMaterial(ResourceMaterial*) override { return true; }
        void DestroyMaterial(ResourceMaterial* m) override { destroyedMaterials.push_back(m); }

        bool CreateGeometry(uint32_t, const Vertex3D*, uint32_t, const uint32_t*,
                            ResourceMesh*) override { return true; }
        void DestroyGeometry(ResourceMesh*) override {}

        bool CreateShader(ResourceShader*) override { return true; }
        void DestroyShader(ResourceShader*) override {}

        std::vector<ResourceTexture*>  destroyedTextures;
        std::vector<ResourceMaterial*> destroyedMaterials;
    };
}

class t_BuiltinResources : public ::testing::Test
{
protected:
    static constexpr uint64_t kMemoryPoolSize = MiB(32);

    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(kMemoryPoolSize);
        created = builtins.Create();
    }

    void TearDown() override
    {
        // ShutdownMemory aborts on a leak, so every Create() must be paired with
        // a Destroy() -- which makes leak detection part of every test here.
        builtins.Destroy(&gpu);
        nous::engine::memory::ShutdownMemory();
    }

    BuiltinResources    builtins;
    RecordingGpuFactory gpu;
    std::vector<std::pair<ResourceType, ResourceBase*>> created;
};

// ---------------------------------------------------------------------------
// What Create() produces
// ---------------------------------------------------------------------------

TEST_F(t_BuiltinResources, CreatesFiveResources)
{
    ASSERT_EQ(created.size(), 5u);
}

TEST_F(t_BuiltinResources, TexturesArePushedBeforeTheMaterial)
{
    // Load-bearing ordering: the material's descriptor set is written on first
    // upload and samples the default texture, so the textures must already be
    // GPU_READY by then.
    ASSERT_EQ(created.size(), 5u);
    EXPECT_EQ(created[0].first, ResourceType::TEXTURE);
    EXPECT_EQ(created[1].first, ResourceType::TEXTURE);
    EXPECT_EQ(created[2].first, ResourceType::TEXTURE);
    EXPECT_EQ(created[3].first, ResourceType::TEXTURE);
    EXPECT_EQ(created[4].first, ResourceType::MATERIAL);
}

TEST_F(t_BuiltinResources, EveryGetterReturnsANonNullResourceAfterCreate)
{
    EXPECT_NE(builtins.GetDefaultTexture(),    nullptr);
    EXPECT_NE(builtins.GetWhiteTexture(),      nullptr);
    EXPECT_NE(builtins.GetBlackTexture(),      nullptr);
    EXPECT_NE(builtins.GetFlatNormalTexture(), nullptr);
    EXPECT_NE(builtins.GetDefaultMaterial(),   nullptr);
}

TEST_F(t_BuiltinResources, TheReturnedPointersAreTheOnesTheGettersExpose)
{
    EXPECT_EQ(created[0].second, builtins.GetDefaultTexture());
    EXPECT_EQ(created[1].second, builtins.GetWhiteTexture());
    EXPECT_EQ(created[2].second, builtins.GetBlackTexture());
    EXPECT_EQ(created[3].second, builtins.GetFlatNormalTexture());
    EXPECT_EQ(created[4].second, builtins.GetDefaultMaterial());
}

TEST_F(t_BuiltinResources, EverythingIsCpuReadyAndAwaitingUpload)
{
    for (const auto& [type, res] : created)
        EXPECT_EQ(res->GetState(), ResourceState::CPU_READY);
}

// ---------------------------------------------------------------------------
// Reserved UIDs
// ---------------------------------------------------------------------------

TEST_F(t_BuiltinResources, ReservedUIDsAreDistinct)
{
    // WriteInstanceSampler dedups descriptor writes by resource UID. Two
    // fallbacks sharing a UID would make the second one's write get skipped.
    std::set<uint32_t> uids;
    uids.insert(builtins.GetDefaultTexture()->GetUID());
    uids.insert(builtins.GetWhiteTexture()->GetUID());
    uids.insert(builtins.GetBlackTexture()->GetUID());
    uids.insert(builtins.GetFlatNormalTexture()->GetUID());

    EXPECT_EQ(uids.size(), 4u);
}

TEST_F(t_BuiltinResources, ReservedUIDsSitAtTheTopOfTheRange)
{
    // They live just below INVALID_ID so they cannot collide with the randomly
    // generated UIDs stored in .meta files.
    EXPECT_EQ(builtins.GetDefaultTexture()->GetUID(),    INVALID_ID - 1);
    EXPECT_EQ(builtins.GetWhiteTexture()->GetUID(),      INVALID_ID - 2);
    EXPECT_EQ(builtins.GetBlackTexture()->GetUID(),      INVALID_ID - 3);
    EXPECT_EQ(builtins.GetFlatNormalTexture()->GetUID(), INVALID_ID - 4);
}

// ---------------------------------------------------------------------------
// Pixel content -- the neutral identities
// ---------------------------------------------------------------------------

TEST_F(t_BuiltinResources, WhiteTextureIsOpaqueWhite)
{
    const ResourceTexture* t = builtins.GetWhiteTexture();

    EXPECT_EQ(t->width, 1u);
    EXPECT_EQ(t->height, 1u);
    EXPECT_EQ(t->channelCount, 4u);
    ASSERT_EQ(t->pixelData.size(), 4u);
    EXPECT_EQ(t->pixelData[0], 255);
    EXPECT_EQ(t->pixelData[1], 255);
    EXPECT_EQ(t->pixelData[2], 255);
    EXPECT_EQ(t->pixelData[3], 255);
}

TEST_F(t_BuiltinResources, BlackTextureIsOpaqueBlack)
{
    // Alpha stays 255: a fully transparent "black" would be wrong for an
    // emissive slot, which samples RGB and ignores alpha.
    const ResourceTexture* t = builtins.GetBlackTexture();

    ASSERT_EQ(t->pixelData.size(), 4u);
    EXPECT_EQ(t->pixelData[0], 0);
    EXPECT_EQ(t->pixelData[1], 0);
    EXPECT_EQ(t->pixelData[2], 0);
    EXPECT_EQ(t->pixelData[3], 255);
}

TEST_F(t_BuiltinResources, FlatNormalTextureDecodesToPositiveZ)
{
    // (128,128,255) maps to roughly (0,0,1) in [-1,1]. Using the white texture
    // here instead would decode to (1,1,1) normalised -- a 45-degree tilt on
    // every surface that falls back to it.
    const ResourceTexture* t = builtins.GetFlatNormalTexture();

    ASSERT_EQ(t->pixelData.size(), 4u);
    EXPECT_EQ(t->pixelData[0], 128);
    EXPECT_EQ(t->pixelData[1], 128);
    EXPECT_EQ(t->pixelData[2], 255);
    EXPECT_EQ(t->pixelData[3], 255);
}

TEST_F(t_BuiltinResources, DefaultTextureIsAFullySizedCheckerboard)
{
    const ResourceTexture* t = builtins.GetDefaultTexture();

    EXPECT_EQ(t->width, 256u);
    EXPECT_EQ(t->height, 256u);
    EXPECT_EQ(t->channelCount, 4u);
    EXPECT_EQ(t->pixelData.size(), 256u * 256u * 4u);
}

TEST_F(t_BuiltinResources, DefaultTextureCheckerboardAlternatesEverySixteenPixels)
{
    // Squares are 16x16. Sampling (0,0) and (16,0) must differ, and (0,0) and
    // (32,0) must match -- a wrong square size or a flipped parity would pass a
    // single-pixel check but fail this.
    const ResourceTexture* t = builtins.GetDefaultTexture();
    const auto& px = t->pixelData;

    auto redAt = [&px](const uint32_t col, const uint32_t row) -> uint8_t
    {
        return px[(row * 256u + col) * 4u];
    };

    EXPECT_NE(redAt(0, 0), redAt(16, 0));
    EXPECT_EQ(redAt(0, 0), redAt(32, 0));
    EXPECT_NE(redAt(0, 0), redAt(0, 16));
    EXPECT_EQ(redAt(0, 0), redAt(16, 16));   // both axes flipped -> back to start
}

TEST_F(t_BuiltinResources, DefaultTextureBlueChannelIsAlwaysSaturated)
{
    // The checkerboard is magenta/blue, not black/white: it is meant to be
    // unmistakable in a scene. Blue is 255 in both squares.
    const ResourceTexture* t = builtins.GetDefaultTexture();

    EXPECT_EQ(t->pixelData[2], 255);
    EXPECT_EQ(t->pixelData[(16u * 256u + 16u) * 4u + 2u], 255);
    EXPECT_EQ(t->pixelData[3], 255);
}

// ---------------------------------------------------------------------------
// Default material
// ---------------------------------------------------------------------------

TEST_F(t_BuiltinResources, DefaultMaterialSamplesTheDefaultTexture)
{
    const ResourceMaterial* m = builtins.GetDefaultMaterial();

    const auto it = m->textureMaps.find("diffuseSampler");
    ASSERT_NE(it, m->textureMaps.end());
    EXPECT_EQ(it->second.texture, builtins.GetDefaultTexture());
}

TEST_F(t_BuiltinResources, DefaultMaterialUniformsAreNeutral)
{
    // Every scalar is 1.0 and every colour is white, so a mesh falling back to
    // this material is lit plainly rather than tinted or blacked out.
    const ResourceMaterial* m = builtins.GetDefaultMaterial();

    for (const char* key : { "diffuseColor", "emissiveColor", "aoIntensity",
                             "normalStrength", "specularIntensity", "shininessScale" })
    {
        const auto it = m->uniformValues.find(key);
        ASSERT_NE(it, m->uniformValues.end()) << "missing uniform: " << key;
        // fdata, not idata: every builtin uniform is a float/vec type, and the
        // two share a union -- reading the wrong member gives garbage.
        EXPECT_FLOAT_EQ(it->second.fdata.x, 1.0f) << "uniform: " << key;
    }
}

// ---------------------------------------------------------------------------
// Destroy
// ---------------------------------------------------------------------------

TEST_F(t_BuiltinResources, DestroyNullsEveryGetter)
{
    builtins.Destroy(&gpu);

    EXPECT_EQ(builtins.GetDefaultTexture(),    nullptr);
    EXPECT_EQ(builtins.GetWhiteTexture(),      nullptr);
    EXPECT_EQ(builtins.GetBlackTexture(),      nullptr);
    EXPECT_EQ(builtins.GetFlatNormalTexture(), nullptr);
    EXPECT_EQ(builtins.GetDefaultMaterial(),   nullptr);

    // TearDown calls Destroy again; nulling is what makes that safe.
}

TEST_F(t_BuiltinResources, DestroyIsIdempotent)
{
    builtins.Destroy(&gpu);
    EXPECT_NO_FATAL_FAILURE(builtins.Destroy(&gpu));
}

TEST_F(t_BuiltinResources, DestroySkipsTheGpuForResourcesThatNeverUploaded)
{
    // Everything is CPU_READY here (no upload ran), so nothing may be handed to
    // the GPU factory -- doing so would call into a dead device during a
    // shutdown that failed before the upload pass.
    builtins.Destroy(&gpu);

    EXPECT_TRUE(gpu.destroyedTextures.empty());
    EXPECT_TRUE(gpu.destroyedMaterials.empty());
}

TEST_F(t_BuiltinResources, DestroyReleasesGpuHandlesForUploadedResources)
{
    // Simulate the upload pass having run.
    for (const auto& [type, res] : created)
        res->SetState(ResourceState::GPU_READY);

    builtins.Destroy(&gpu);

    EXPECT_EQ(gpu.destroyedTextures.size(),  4u);
    EXPECT_EQ(gpu.destroyedMaterials.size(), 1u);
}

TEST_F(t_BuiltinResources, DestroyReleasesOnlyTheUploadedSubset)
{
    // A partial upload is the realistic shutdown-during-load case.
    builtins.GetWhiteTexture()->SetState(ResourceState::GPU_READY);

    builtins.Destroy(&gpu);

    ASSERT_EQ(gpu.destroyedTextures.size(), 1u);
    EXPECT_TRUE(gpu.destroyedMaterials.empty());
}
