#include <gtest/gtest.h>

#include "Engine/Systems/ResourceManager/ScenePreloader/include/ScenePreloader.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Importer/IImporterManager.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Systems/ResourceManager/Resource/MetaFileData.inl"

#include <filesystem>
#include <fstream>
#include <future>
#include <string>
#include <vector>

// =====================================================
// Mock
// =====================================================

class SpMockImporterManager : public IImporterManager
{
public:
    void Init(ModuleResourceManager*) override                                    {}
    bool Import(ResourceType, const MetaFileData&) override                       { return true; }
    bool Deserialize(ResourceType, const std::string&, Resource*) override        { return true; }
    void Evict(ResourceType, Resource*) override                                  {}
    bool Upload(ResourceType, Resource*, IGPUResourceFactory*) override           { return true; }
    void Release(ResourceType, Resource*, IGPUResourceFactory*) override          {}
};

// =====================================================
// Scene file helpers
// =====================================================

// Represents a single CMesh component entry inside a scene file.
struct FakeCMeshEntry
{
    std::string assetPath;
    std::string libraryPath;  // empty = omit from JSON
    uint32_t    uid          = 0;
    int32_t     submeshIndex = -1;
};

// Writes a minimal .nous JSON file with the given list of CMesh entries.
// Each entry is placed inside its own GameObject.
static void WriteSceneFile(const std::filesystem::path& path,
                            const std::vector<FakeCMeshEntry>& entries)
{
    if (const auto parent = path.parent_path(); !parent.empty())
        std::filesystem::create_directories(parent);
    std::ofstream f(path);
    if (!f.is_open()) return;

    f << "{\n  \"GameObjects\": [\n";

    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& e = entries[i];
        f << "    {\n      \"components\": [\n";
        f << "        {\n";
        f << "          \"type\": \"CMesh\",\n";
        f << "          \"assetPath\": \"" << e.assetPath << "\"";
        if (!e.libraryPath.empty())
            f << ",\n          \"libraryPath\": \"" << e.libraryPath << "\"";
        if (e.uid != 0)
            f << ",\n          \"resourceUID\": " << e.uid;
        if (e.submeshIndex >= 0)
            f << ",\n          \"submeshIndex\": " << e.submeshIndex;
        f << "\n        }\n      ]\n    }";
        if (i + 1 < entries.size()) f << ",";
        f << "\n";
    }

    f << "  ]\n}\n";
}

// =====================================================
// Fixture
// =====================================================

class t_ScenePreloader : public ::testing::Test
{
protected:
    static constexpr uint64 kMemoryPoolSize = MiB(64);

    EventSystem*                         eventSystem = nullptr;
    NOUS_Multithreading::NOUS_JobSystem* jobSystem   = nullptr;
    SpMockImporterManager                mockImporter;
    ModuleResourceManager*               rm          = nullptr;
    ScenePreloader*                      preloader   = nullptr;

    std::filesystem::path tempDir;
    std::filesystem::path savedCwd;

    void SetUp() override
    {
        MemoryManager::InitializeMemory(kMemoryPoolSize);
        eventSystem = new EventSystem();
        jobSystem   = new NOUS_Multithreading::NOUS_JobSystem(0); // inline execution
        rm          = new ModuleResourceManager(eventSystem, jobSystem, &mockImporter);
        preloader   = new ScenePreloader(rm);

        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        tempDir = std::filesystem::temp_directory_path()
                / ("nous_sp_" + std::string(info->test_suite_name()) + "_" + info->name());
        std::filesystem::create_directories(tempDir);
        savedCwd = std::filesystem::current_path();
        std::filesystem::current_path(tempDir);
    }

    void TearDown() override
    {
        std::filesystem::current_path(savedCwd);
        std::filesystem::remove_all(tempDir);

        delete preloader;
        preloader = nullptr;

        CleanupAllResources();
        delete rm;
        rm = nullptr;
        delete jobSystem;
        jobSystem = nullptr;
        delete eventSystem;
        eventSystem = nullptr;

        MemoryManager::ShutdownMemory();
    }

    // Evicts every resource still in the map so ShutdownMemory finds zero outstanding allocations.
    void CleanupAllResources()
    {
        rm->TakePendingUploads();

        auto snapshot = rm->GetResourcesMap();
        for (auto& [uid, res] : snapshot)
        {
            if (!res) continue;
            while (res->GetReferenceCount() > 0)
                rm->UnloadResource(uid);
        }

        auto releases = rm->TakePendingReleases();
        for (auto& [type, res] : releases)
            rm->EvictResource(type, res);

        snapshot = rm->GetResourcesMap();
        for (auto& [uid, res] : snapshot)
        {
            if (res && res->GetReferenceCount() == 0)
                rm->EvictResource(res->GetType(), res);
        }
    }

    // Waits for all futures returned by PreloadSceneResourcesAsync (safe with inline JobSystem).
    static void WaitAll(std::vector<std::future<void>>& futures)
    {
        for (auto& f : futures) f.wait();
    }
};

// =====================================================
// Error / empty-scene guards
// =====================================================

TEST_F(t_ScenePreloader, MissingSceneFileReturnsEmptyFutures)
{
    auto futures = preloader->PreloadSceneResourcesAsync(jobSystem, "nonexistent.nous");
    EXPECT_TRUE(futures.empty());
}

TEST_F(t_ScenePreloader, EmptyGameObjectsArrayReturnsEmptyFutures)
{
    const std::filesystem::path scene = "empty_gos.nous";
    std::ofstream(scene) << "{\"GameObjects\": []}";

    auto futures = preloader->PreloadSceneResourcesAsync(jobSystem, scene.string());
    EXPECT_TRUE(futures.empty());
}

TEST_F(t_ScenePreloader, MissingGameObjectsKeyReturnsEmptyFutures)
{
    const std::filesystem::path scene = "no_gos_key.nous";
    std::ofstream(scene) << "{}";

    auto futures = preloader->PreloadSceneResourcesAsync(jobSystem, scene.string());
    EXPECT_TRUE(futures.empty());
}

TEST_F(t_ScenePreloader, NoMeshComponentsReturnsEmptyFutures)
{
    const std::filesystem::path scene = "no_mesh.nous";
    // GameObject exists but has no CMesh components.
    std::ofstream(scene) << "{\n  \"GameObjects\": [\n"
                            "    {\"components\": [{\"type\": \"CTransform\"}]}\n"
                            "  ]\n}";

    auto futures = preloader->PreloadSceneResourcesAsync(jobSystem, scene.string());
    EXPECT_TRUE(futures.empty());
}

TEST_F(t_ScenePreloader, MeshWithEmptyAssetPathIsSkipped)
{
    // CMesh entry with assetPath == "" must be ignored (no path to load from).
    const std::filesystem::path scene = "empty_path.nous";
    std::ofstream(scene) << "{\n  \"GameObjects\": [\n"
                            "    {\"components\": [{\"type\": \"CMesh\", \"assetPath\": \"\"}]}\n"
                            "  ]\n}";

    auto futures = preloader->PreloadSceneResourcesAsync(jobSystem, scene.string());
    EXPECT_TRUE(futures.empty());
}

// =====================================================
// Future count — deduplication and per-entry scheduling
// =====================================================

TEST_F(t_ScenePreloader, OneUniqueMeshYieldsOneFuture)
{
    const std::filesystem::path scene = "one_mesh.nous";
    WriteSceneFile(scene, {{"Assets/Meshes/a.fbx", "", 0, 0}});

    auto futures = preloader->PreloadSceneResourcesAsync(jobSystem, scene.string());
    EXPECT_EQ(futures.size(), 1u);
}

TEST_F(t_ScenePreloader, TwoDistinctMeshesYieldTwoFutures)
{
    const std::filesystem::path scene = "two_meshes.nous";
    WriteSceneFile(scene, {
        {"Assets/Meshes/a.fbx", "", 0, 0},
        {"Assets/Meshes/b.fbx", "", 0, 0},
    });

    auto futures = preloader->PreloadSceneResourcesAsync(jobSystem, scene.string());
    EXPECT_EQ(futures.size(), 2u);
}

TEST_F(t_ScenePreloader, DuplicateMeshEntriesAreDeduplicatedToOneFuture)
{
    // Same assetPath + submeshIndex in two different GameObjects → 1 unique request.
    const std::filesystem::path scene = "dedup.nous";
    WriteSceneFile(scene, {
        {"Assets/Meshes/same.fbx", "", 0, 0},
        {"Assets/Meshes/same.fbx", "", 0, 0},
    });

    auto futures = preloader->PreloadSceneResourcesAsync(jobSystem, scene.string());
    EXPECT_EQ(futures.size(), 1u);
}

TEST_F(t_ScenePreloader, SameAssetDifferentSubmeshIndicesAreNotDeduped)
{
    // Same assetPath but different submeshIndex → two distinct cache keys → two futures.
    const std::filesystem::path scene = "two_submeshes.nous";
    WriteSceneFile(scene, {
        {"Assets/Meshes/model.fbx", "", 0, 0},
        {"Assets/Meshes/model.fbx", "", 0, 1},
    });

    auto futures = preloader->PreloadSceneResourcesAsync(jobSystem, scene.string());
    EXPECT_EQ(futures.size(), 2u);
}

// =====================================================
// Future resolution
// =====================================================

TEST_F(t_ScenePreloader, AllFuturesResolveWithInlineJobSystem)
{
    // Inline job system (0 threads) must resolve all futures before returning.
    // Calling wait() here verifies no future is left pending (which would deadlock
    // with a real thread pool but should be instant with inline execution).
    const std::filesystem::path scene = "resolve.nous";
    WriteSceneFile(scene, {
        {"Assets/Meshes/a.fbx", "", 0, 0},
        {"Assets/Meshes/b.fbx", "", 0, 1},
    });

    auto futures = preloader->PreloadSceneResourcesAsync(jobSystem, scene.string());
    ASSERT_EQ(futures.size(), 2u);
    WaitAll(futures); // must not block
}

TEST_F(t_ScenePreloader, FuturesResolveEvenWhenAssetsAreMissing)
{
    // All load attempts will fail (no .meta, no .nmesh on disk), but each job
    // must still call prom->set_value() so the future is resolved.
    const std::filesystem::path scene = "missing_assets.nous";
    WriteSceneFile(scene, {
        {"Assets/Meshes/ghost.fbx", "Library/Meshes/ghost.nmesh", 42u, 0},
    });

    auto futures = preloader->PreloadSceneResourcesAsync(jobSystem, scene.string());
    ASSERT_EQ(futures.size(), 1u);
    WaitAll(futures); // must not block even though the load failed
}
