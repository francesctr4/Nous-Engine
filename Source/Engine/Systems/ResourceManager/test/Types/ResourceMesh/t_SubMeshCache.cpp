#include <gtest/gtest.h>

#include <ResourceManager/Import/ModelParser/ModelImportData.h>
#include <ResourceManager/Types/ResourceMesh/ImporterMesh.h>
#include <ResourceManager/Types/ResourceMesh/SubMeshCache.h>
#include <ResourceManager/Types/ResourceMesh/ResourceMesh.h>
#include <ResourceManager/Core/MetaFileData.h>
#include <ResourceManager/Core/ResourceQueue.h>
#include <ResourceManager/Core/ResourceTable.h>
#include <MemoryManager/MemoryManager.h>
#include <Utils/Math/Vertex.inl>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <utility>
#include <vector>

// =====================================================
// Binary / meta helpers
// =====================================================

// Writes a minimal .nmesh with one trivial vertex+index per submesh. The names
// vector determines how many submeshes are in the file.
//
// Goes through ImporterMesh's own writer rather than hand-rolling the byte layout.
// The previous version wrote the header by hand using the LIVE sizeof(Vertex3D),
// which stopped describing the real format the moment Vertex3D gained bone data —
// and would have kept "passing" while feeding the reader a stride it no longer
// used. Round-tripping through the writer cannot drift from the reader.
static void WriteMinimalNmeshFile(const std::filesystem::path& path,
                                   const std::vector<std::string>& submeshNames)
{
    std::filesystem::create_directories(path.parent_path());

    nous::engine::resource_manager::ModelImportData model;
    model.submeshes.reserve(submeshNames.size());

    for (const auto& name : submeshNames)
    {
        SubMeshData sub;
        sub.name           = name;
        sub.localTransform = glm::mat4(1.0f);

        Vertex3D v{};
        v.position = glm::vec3(1.0f, 0.0f, 0.0f);
        sub.vertices.push_back(v);
        sub.indices.push_back(0u);

        model.submeshes.push_back(std::move(sub));
    }

    MetaFileData meta;
    meta.libraryPath = path.string();

    ImporterMesh::SaveModel(meta, model);   // test fails later if this did not land
}

// Writes a minimal JSON .meta file for a MESH asset (ResourceType::MESH == 0).
static void WriteMetaFile(const std::string& assetsPath,
                           uint32_t uid,
                           const std::string& libraryPath)
{
    std::filesystem::create_directories(std::filesystem::path(assetsPath).parent_path());
    std::ofstream f(assetsPath + ".meta");
    if (!f.is_open()) return;
    const auto name = std::filesystem::path(assetsPath).stem().string();
    f << "{\n"
      << "  \"Name\": \"" << name << "\",\n"
      << "  \"UID\": " << uid << ",\n"
      << "  \"Resource Type\": 0,\n"
      << "  \"Assets Path\": \"" << assetsPath << "\",\n"
      << "  \"Library Path\": \"" << libraryPath << "\"\n"
      << "}\n";
}

// =====================================================
// Fixture
// =====================================================

class t_SubMeshCache : public ::testing::Test
{
protected:
    static constexpr uint64_t kMemoryPoolSize = MiB(32);

    // SubMeshCache owns references to these; they must outlive the cache object.
    ResourceTable         table;
    ResourceQueue         pendingUploads;

    SubMeshCache*         cache    = nullptr;
    std::filesystem::path tempDir;
    std::filesystem::path savedCwd;

    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(kMemoryPoolSize);
        cache = new SubMeshCache(table, pendingUploads);

        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        tempDir = std::filesystem::temp_directory_path()
                / ("nous_smc_" + std::string(info->test_suite_name()) + "_" + info->name());
        std::filesystem::create_directories(tempDir);
        savedCwd = std::filesystem::current_path();
        std::filesystem::current_path(tempDir);
    }

    void TearDown() override
    {
        std::filesystem::current_path(savedCwd);
        std::filesystem::remove_all(tempDir);

        cache->Clear();
        delete cache;
        cache = nullptr;

        // SubMeshCache only allocates ResourceMesh objects. Delete them all.
        for (auto& [uid, res] : table.Snapshot())
            if (res) NOUS_DELETE(res, MemoryTag::RESOURCE_MESH);
        table.Clear();

        nous::engine::memory::ShutdownMemory();
    }
};

// =====================================================
// RequestOrCreateFromLibrary — error guards
// =====================================================

TEST_F(t_SubMeshCache, FromLibrary_MissingFileReturnsNull)
{
    ResourceMesh* result = cache->RequestOrCreateFromLibrary("nonexistent.nmesh", 0);
    EXPECT_EQ(result, nullptr);
}

TEST_F(t_SubMeshCache, FromLibrary_OutOfRangeIndexReturnsNull)
{
    WriteMinimalNmeshFile("Library/Meshes/single.nmesh", {"OnlySubmesh"});
    ResourceMesh* result = cache->RequestOrCreateFromLibrary("Library/Meshes/single.nmesh", 5);
    EXPECT_EQ(result, nullptr);
}

// =====================================================
// RequestOrCreateFromLibrary — first load
// =====================================================

TEST_F(t_SubMeshCache, FromLibrary_FirstCallRegistersInResourcesMap)
{
    WriteMinimalNmeshFile("Library/Meshes/test.nmesh", {"Sub0"});

    ResourceMesh* mesh = cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0);

    ASSERT_NE(mesh, nullptr);
    EXPECT_TRUE(table.Contains(mesh->GetUID()));
}

TEST_F(t_SubMeshCache, FromLibrary_FirstCallSetsRefCountToOne)
{
    WriteMinimalNmeshFile("Library/Meshes/test.nmesh", {"Sub0"});

    ResourceMesh* mesh = cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0);

    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->GetReferenceCount(), 1u);
}

TEST_F(t_SubMeshCache, FromLibrary_FirstCallQueuesPendingUpload)
{
    WriteMinimalNmeshFile("Library/Meshes/test.nmesh", {"Sub0"});

    cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0);

    EXPECT_EQ(pendingUploads.Size(), 1u);
}

TEST_F(t_SubMeshCache, FromLibrary_FirstCallSetsStateToGpuReady)
{
    WriteMinimalNmeshFile("Library/Meshes/test.nmesh", {"Sub0"});

    ResourceMesh* mesh = cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0);

    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->GetState(), ResourceState::CPU_READY);
}

// =====================================================
// RequestOrCreateFromLibrary — cache hit
// =====================================================

TEST_F(t_SubMeshCache, FromLibrary_CacheHitReturnsSamePointer)
{
    WriteMinimalNmeshFile("Library/Meshes/test.nmesh", {"Sub0"});

    ResourceMesh* first  = cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0);
    ResourceMesh* second = cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0);

    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first, second);
}

TEST_F(t_SubMeshCache, FromLibrary_CacheHitBumpsRefCount)
{
    WriteMinimalNmeshFile("Library/Meshes/test.nmesh", {"Sub0"});

    ResourceMesh* mesh = cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0);
    cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0); // cache hit

    ASSERT_NE(mesh, nullptr);
    EXPECT_EQ(mesh->GetReferenceCount(), 2u);
}

TEST_F(t_SubMeshCache, FromLibrary_CacheHitDoesNotQueueSecondUpload)
{
    WriteMinimalNmeshFile("Library/Meshes/test.nmesh", {"Sub0"});

    cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0);
    cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0); // cache hit

    EXPECT_EQ(pendingUploads.Size(), 1u);
}

// =====================================================
// RequestOrCreateFromLibrary — assetsPath back-fill
// =====================================================

TEST_F(t_SubMeshCache, FromLibrary_BackfillsAssetsPathOnCacheHit)
{
    WriteMinimalNmeshFile("Library/Meshes/test.nmesh", {"Sub0"});

    // First call: GAME-mode path, no assetsPath.
    ResourceMesh* mesh = cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0);
    ASSERT_NE(mesh, nullptr);
    EXPECT_TRUE(mesh->GetAssetsPath().empty());

    // Second call: EDITOR-mode path provides the assetsPath — should be back-filled.
    cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0, "Assets/Meshes/test.fbx");
    EXPECT_EQ(mesh->GetAssetsPath(), "Assets/Meshes/test.fbx");
}

// =====================================================
// RequestOrCreateFromLibrary — stale cache entry
// =====================================================

TEST_F(t_SubMeshCache, FromLibrary_StaleEntryIsRebuiltIntoNewResource)
{
    WriteMinimalNmeshFile("Library/Meshes/test.nmesh", {"Sub0"});

    ResourceMesh* first = cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0);
    ASSERT_NE(first, nullptr);

    // Simulate eviction: remove from the primary resources map and free the object.
    // The index entry in SubMeshCache still exists (stale).
    table.Erase(first->GetUID());
    NOUS_DELETE(first, MemoryTag::RESOURCE_MESH);

    // Second call: cache detects the stale entry and rebuilds.
    ResourceMesh* second = cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0);

    ASSERT_NE(second, nullptr);
    EXPECT_TRUE(table.Contains(second->GetUID()));
}

// =====================================================
// RequestOrCreateFromLibrary — multiple submesh indices
// =====================================================

TEST_F(t_SubMeshCache, FromLibrary_DifferentIndicesProduceSeparateCacheEntries)
{
    WriteMinimalNmeshFile("Library/Meshes/multi.nmesh", {"Sub0", "Sub1"});

    ResourceMesh* sub0 = cache->RequestOrCreateFromLibrary("Library/Meshes/multi.nmesh", 0);
    ResourceMesh* sub1 = cache->RequestOrCreateFromLibrary("Library/Meshes/multi.nmesh", 1);

    ASSERT_NE(sub0, nullptr);
    ASSERT_NE(sub1, nullptr);
    EXPECT_NE(sub0, sub1);
    EXPECT_EQ(table.Snapshot().size(), 2u);
    EXPECT_EQ(pendingUploads.Size(), 2u);
}

// =====================================================
// RequestOrCreate (EDITOR path via .meta file)
// =====================================================

TEST_F(t_SubMeshCache, EditorPath_NoMetaFileReturnsNull)
{
    ResourceMesh* result = cache->RequestOrCreate("Assets/Meshes/ghost.fbx", 0);
    EXPECT_EQ(result, nullptr);
}

TEST_F(t_SubMeshCache, EditorPath_WithMetaAndLibraryBuildsAndRegisters)
{
    WriteMinimalNmeshFile("Library/Meshes/1001.nmesh", {"Sub0"});
    WriteMetaFile("Assets/Meshes/test.fbx", 1001u, "Library/Meshes/1001.nmesh");

    ResourceMesh* mesh = cache->RequestOrCreate("Assets/Meshes/test.fbx", 0);

    ASSERT_NE(mesh, nullptr);
    EXPECT_TRUE(table.Contains(mesh->GetUID()));
    EXPECT_EQ(mesh->GetAssetsPath(), "Assets/Meshes/test.fbx");
}

TEST_F(t_SubMeshCache, EditorPath_CacheHitReturnsSamePointer)
{
    WriteMinimalNmeshFile("Library/Meshes/1002.nmesh", {"Sub0"});
    WriteMetaFile("Assets/Meshes/shared.fbx", 1002u, "Library/Meshes/1002.nmesh");

    ResourceMesh* first  = cache->RequestOrCreate("Assets/Meshes/shared.fbx", 0);
    ResourceMesh* second = cache->RequestOrCreate("Assets/Meshes/shared.fbx", 0);

    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first, second);
    EXPECT_EQ(first->GetReferenceCount(), 2u);
}

// =====================================================
// EraseUID
// =====================================================

TEST_F(t_SubMeshCache, EraseUID_ForcesRebuildOnNextRequest)
{
    WriteMinimalNmeshFile("Library/Meshes/test.nmesh", {"Sub0"});

    ResourceMesh* first = cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0);
    ASSERT_NE(first, nullptr);
    const uint32_t firstUID = first->GetUID();

    // Erase the index entry under the required lock (the table's mutex).
    { ResourceTable::ScopedLock lock(table); cache->EraseUID(firstUID); }

    // Next call has no cache entry — must rebuild a new resource.
    ResourceMesh* second = cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0);

    ASSERT_NE(second, nullptr);
    EXPECT_NE(first, second);
}

// =====================================================
// Clear
// =====================================================

TEST_F(t_SubMeshCache, Clear_DropsCacheAndForcesRebuildOnNextRequest)
{
    WriteMinimalNmeshFile("Library/Meshes/test.nmesh", {"Sub0"});

    ResourceMesh* first = cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0);
    ASSERT_NE(first, nullptr);

    cache->Clear();

    ResourceMesh* second = cache->RequestOrCreateFromLibrary("Library/Meshes/test.nmesh", 0);

    ASSERT_NE(second, nullptr);
    EXPECT_NE(first, second);
}
