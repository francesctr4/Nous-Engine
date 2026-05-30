#include <gtest/gtest.h>

#include "Engine/Systems/ResourceManager/Runtime/HotReloader/include/HotReloader.h"
#include "Engine/Systems/ResourceManager/Core/ImporterManager/include/IImporterDispatcher.h"
#include "Engine/Systems/ResourceManager/Core/Resource/include/MetaFileData.h"
#include "Engine/Systems/ResourceManager/Core/Resource/include/Resource.h"
#include "Engine/Systems/ResourceManager/Core/TypeRegistry/include/TypeRegistry.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"

#include <filesystem>
#include <fstream>
#include <string>

// =====================================================
// Mock
// =====================================================

// HotReloader depends only on the import dispatch seam, so the mock stubs the
// single Import method rather than the full IImporterManager lifecycle.
class MockImporterManager : public IImporterDispatcher
{
public:
    int                       importCallCount = 0;
    std::string               lastImportedPath;
    ResourceType              lastImportedType = ResourceType::UNKNOWN;
    bool                      importShouldSucceed = true;

    bool Import(ResourceType type, const MetaFileData& meta) override
    {
        ++importCallCount;
        lastImportedPath = meta.assetsPath;
        lastImportedType = type;
        return importShouldSucceed;
    }
};

// =====================================================
// .meta helper — matches the JSON format ImportPipeline::ReadMetaFile expects.
// =====================================================

static void WriteMetaFile(const std::string& assetsPath,
                           uint32_t uid,
                           int resourceType,
                           const std::string& libraryPath)
{
    std::filesystem::create_directories(std::filesystem::path(assetsPath).parent_path());
    std::ofstream f(assetsPath + ".meta");
    const auto name = std::filesystem::path(assetsPath).stem().string();
    f << "{\n"
      << "  \"Name\": \"" << name << "\",\n"
      << "  \"UID\": " << uid << ",\n"
      << "  \"Resource Type\": " << resourceType << ",\n"
      << "  \"Assets Path\": \"" << assetsPath << "\",\n"
      << "  \"Library Path\": \"" << libraryPath << "\"\n"
      << "}\n";
}

// =====================================================
// Fixture
// =====================================================

class t_HotReloader : public ::testing::Test
{
protected:
    static constexpr uint64 kMemoryPoolSize = MiB(32);

    MockImporterManager   mockImporter;
    nous::engine::multithreading::NOUS_JobSystem* jobSystem = nullptr;
    TypeRegistry*         registry = nullptr;
    HotReloader*          hr       = nullptr;

    std::filesystem::path tempDir;
    std::filesystem::path savedCwd;

    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(kMemoryPoolSize);

        // RegisterResourceTypes sets the hotReloadable flag on TEXTURE/MATERIAL
        // and not on MESH/AUDIO/SHADER — this is exactly the policy under test.
        registry = new TypeRegistry();
        RegisterResourceTypes(*registry);

        // Worker count 0 -> jobs run inline on SubmitJob, so DispatchReimportJob
        // completes synchronously before returning. Lets us assert post-conditions
        // without sleeping or polling.
        jobSystem = new nous::engine::multithreading::NOUS_JobSystem(0);

        hr = new HotReloader(&mockImporter, *registry, jobSystem);

        // Each test gets a fresh tempDir and chdir into it. The worker reads
        // .meta paths relative to cwd, so this isolates filesystem state.
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        tempDir = std::filesystem::temp_directory_path()
            / ("nous_hr_" + std::string(info->test_suite_name()) + "_" + info->name());
        std::filesystem::create_directories(tempDir);
        savedCwd = std::filesystem::current_path();
        std::filesystem::current_path(tempDir);
    }

    void TearDown() override
    {
        hr->WaitForInFlight();
        delete hr; hr = nullptr;
        delete jobSystem; jobSystem = nullptr;

        delete registry; registry = nullptr;

        std::filesystem::current_path(savedCwd);
        std::filesystem::remove_all(tempDir);

        nous::engine::memory::ShutdownMemory();
    }
};

// =====================================================
// Enabled-state lifecycle
// =====================================================

TEST_F(t_HotReloader, Constructed_IsEnabledByDefault)
{
    EXPECT_TRUE(hr->IsEnabled());
}

TEST_F(t_HotReloader, Disable_FlipsToFalse)
{
    hr->Disable();
    EXPECT_FALSE(hr->IsEnabled());
}

// =====================================================
// Hot-reloadable type policy (the integration with TypeDescriptor::hotReloadable)
// =====================================================

TEST_F(t_HotReloader, Track_HotReloadableType_DispatchTriggersImport)
{
    // TEXTURE has hotReloadable=true in RegisterResourceTypes.
    const std::string assetsPath  = "Assets/Textures/foo.png";
    const std::string libraryPath = "Library/Textures/42.png";
    WriteMetaFile(assetsPath, 42u, static_cast<int>(ResourceType::TEXTURE), libraryPath);

    hr->TrackAsset(assetsPath, 42u, ResourceType::TEXTURE);
    hr->DispatchReimportJob(assetsPath);

    EXPECT_EQ(mockImporter.importCallCount, 1);
    EXPECT_EQ(mockImporter.lastImportedType, ResourceType::TEXTURE);
}

TEST_F(t_HotReloader, Track_NonHotReloadableType_DispatchDoesNothing)
{
    // MESH has hotReloadable=false — TrackAsset is filtered out by the
    // registry lookup, so dispatch finds nothing tracked.
    const std::string assetsPath = "Assets/Meshes/bar.fbx";
    WriteMetaFile(assetsPath, 43u, static_cast<int>(ResourceType::MESH), "Library/Meshes/43.nmesh");

    hr->TrackAsset(assetsPath, 43u, ResourceType::MESH);
    hr->DispatchReimportJob(assetsPath);

    EXPECT_EQ(mockImporter.importCallCount, 0);
}

// =====================================================
// Tracked-map lifecycle
// =====================================================

TEST_F(t_HotReloader, UntrackedPath_DispatchDoesNothing)
{
    hr->DispatchReimportJob("Assets/Textures/ghost.png");
    EXPECT_EQ(mockImporter.importCallCount, 0);
}

TEST_F(t_HotReloader, Untrack_AfterTrack_DispatchDoesNothing)
{
    const std::string assetsPath = "Assets/Textures/temp.png";
    WriteMetaFile(assetsPath, 44u, static_cast<int>(ResourceType::TEXTURE), "Library/Textures/44.png");

    hr->TrackAsset(assetsPath, 44u, ResourceType::TEXTURE);
    hr->UntrackAsset(assetsPath);
    hr->DispatchReimportJob(assetsPath);

    EXPECT_EQ(mockImporter.importCallCount, 0);
}

TEST_F(t_HotReloader, Disabled_TrackAsset_IsNoOp)
{
    // Disable mirrors the production "game mode" path. Track must become a
    // no-op so the same code path (LoadResourceIntoSlot) can call it
    // unconditionally without leaking into the game build.
    const std::string assetsPath = "Assets/Textures/quiet.png";
    WriteMetaFile(assetsPath, 45u, static_cast<int>(ResourceType::TEXTURE), "Library/Textures/45.png");

    hr->Disable();
    hr->TrackAsset(assetsPath, 45u, ResourceType::TEXTURE);
    hr->DispatchReimportJob(assetsPath);

    EXPECT_EQ(mockImporter.importCallCount, 0);
}

// =====================================================
// Worker -> main-thread ready-upload queue
// =====================================================

TEST_F(t_HotReloader, TakeReadyUploads_EmptyByDefault)
{
    EXPECT_TRUE(hr->TakeReadyUploads().empty());
}

TEST_F(t_HotReloader, DispatchSuccess_QueuesReadyUpload)
{
    const std::string assetsPath = "Assets/Textures/queued.png";
    WriteMetaFile(assetsPath, 46u, static_cast<int>(ResourceType::TEXTURE), "Library/Textures/46.png");

    hr->TrackAsset(assetsPath, 46u, ResourceType::TEXTURE);
    hr->DispatchReimportJob(assetsPath);

    auto ready = hr->TakeReadyUploads();
    ASSERT_EQ(ready.size(), 1u);
    EXPECT_EQ(ready[0].uid,  46u);
    EXPECT_EQ(ready[0].type, ResourceType::TEXTURE);

    // Second drain must be empty.
    EXPECT_TRUE(hr->TakeReadyUploads().empty());
}
