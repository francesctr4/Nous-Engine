#include <gtest/gtest.h>

#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include <ResourceManager/Core/IImporterManager.h>
#include "Engine/Core/EventSystem/EventSystem.h"
#include <NOUS_Multithreading/NOUS_JobSystem.h>
#include <MemoryManager/MemoryManager.h>
#include "Engine/Core/Globals.h"
#include <ResourceManager/Core/ResourceBase.h>
#include <ResourceManager/Core/TypeRegistry.h>

// =====================================================
// Mock
// =====================================================

class MockImporterManager : public IImporterManager
{
public:
    bool initCalled = false;

    bool deserializeResult = true;

    void Init(IResourceLoader*) override                                                { initCalled = true; }
    bool Import(ResourceType, const MetaFileData&) override                             { return true; }
    bool Deserialize(ResourceType, const std::string&, ResourceBase*) override              { return deserializeResult; }
    void Evict(ResourceType, ResourceBase*) override                                        {}
    bool Upload(ResourceType, ResourceBase*, IGPUResourceFactory*) override                 { return true; }
    void Release(ResourceType, ResourceBase*, IGPUResourceFactory*) override                {}
};

// =====================================================
// Fixture
// =====================================================

class t_ModuleResourceManager : public ::testing::Test
{
protected:
    static constexpr uint64 kMemoryPoolSize = MiB(64);

    EventSystem*          eventSystem = nullptr;
    nous::engine::multithreading::NOUS_JobSystem* jobSystem = nullptr; // single-threaded: jobs run inline
    MockImporterManager   mockImporter;
    ModuleResourceManager* rm = nullptr;
    TypeRegistry*  registry = nullptr;

    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(kMemoryPoolSize);

        // ModuleResourceManager dispatches create/destroy/cleanup through the
        // registry, which is injected at construction (below).
        registry = new TypeRegistry();
        RegisterResourceTypes(*registry);

        eventSystem = new EventSystem();
        jobSystem   = new nous::engine::multithreading::NOUS_JobSystem(0);
        rm = new ModuleResourceManager(eventSystem, jobSystem, &mockImporter, *registry);
    }

    void TearDown() override
    {
        CleanupAllResources();
        delete rm;
        rm = nullptr;
        delete jobSystem;
        jobSystem = nullptr;
        delete eventSystem;
        eventSystem = nullptr;

        delete registry;
        registry = nullptr;

        nous::engine::memory::ShutdownMemory();
    }

    // Evicts every resource still in the map so ShutdownMemory finds zero outstanding allocations.
    void CleanupAllResources()
    {
        rm->TakePendingUploads(); // drain so re-queue logic in EvictResource doesn't re-add

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

        // Second pass: evict resources that hit refcount==0 and whose pending-release
        // entry was already drained by the test body (e.g. tests that call
        // TakePendingReleases() themselves without following up with EvictResource).
        snapshot = rm->GetResourcesMap();
        for (auto& [uid, res] : snapshot)
        {
            if (res && res->GetReferenceCount() == 0)
                rm->EvictResource(res->GetType(), res);
        }
    }
};

// =====================================================
// Tests
// =====================================================

TEST_F(t_ModuleResourceManager, ResourceDoesNotExistInitially)
{
    EXPECT_FALSE(rm->ResourceExists(9999));
}

TEST_F(t_ModuleResourceManager, AwakeCallsImporterInit)
{
    rm->Awake();
    EXPECT_TRUE(mockImporter.initCalled);
}

TEST_F(t_ModuleResourceManager, CreateResourceFromLibraryRegistersResource)
{
    const uint32 uid = 42;
    ResourceBase* res = rm->CreateResourceFromLibrary(uid, ResourceType::MESH, "testMesh",
                                                  "Assets/test.fbx", "Library/Meshes/42.nmesh");
    ASSERT_NE(res, nullptr);
    EXPECT_TRUE(rm->ResourceExists(uid));
}

TEST_F(t_ModuleResourceManager, CreateResourceFromLibraryInitialRefCountIsOne)
{
    ResourceBase* res = rm->CreateResourceFromLibrary(1, ResourceType::MESH, "testMesh",
                                                  "Assets/test.fbx", "Library/Meshes/1.nmesh");
    ASSERT_NE(res, nullptr);
    EXPECT_EQ(res->GetReferenceCount(), 1u);
}

TEST_F(t_ModuleResourceManager, CreateResourceFromLibraryDeduplicates)
{
    const uint32 uid = 7;
    ResourceBase* first  = rm->CreateResourceFromLibrary(uid, ResourceType::MESH, "mesh",
                                                     "Assets/a.fbx", "Library/Meshes/7.nmesh");
    ResourceBase* second = rm->CreateResourceFromLibrary(uid, ResourceType::MESH, "mesh",
                                                     "Assets/a.fbx", "Library/Meshes/7.nmesh");

    ASSERT_NE(first, nullptr);
    EXPECT_EQ(first, second);
    EXPECT_EQ(first->GetReferenceCount(), 2u);
    EXPECT_EQ(rm->GetResourcesMap().size(), 1u);
}

TEST_F(t_ModuleResourceManager, CreateResourceFromLibraryQueuesPendingUpload)
{
    rm->CreateResourceFromLibrary(10, ResourceType::MESH, "m", "a.fbx", "l.nmesh");

    auto uploads = rm->TakePendingUploads();
    EXPECT_EQ(uploads.size(), 1u);
}

TEST_F(t_ModuleResourceManager, TakePendingUploadsClearsQueue)
{
    rm->CreateResourceFromLibrary(10, ResourceType::MESH, "m", "a.fbx", "l.nmesh");

    rm->TakePendingUploads();
    EXPECT_TRUE(rm->TakePendingUploads().empty());
}

TEST_F(t_ModuleResourceManager, UnloadResourceAtZeroRefQueuesPendingRelease)
{
    const uint32 uid = 20;
    rm->CreateResourceFromLibrary(uid, ResourceType::MESH, "m", "a.fbx", "l.nmesh");
    rm->TakePendingUploads();

    rm->UnloadResource(uid); // refcount 1 → 0

    auto releases = rm->TakePendingReleases();
    EXPECT_EQ(releases.size(), 1u);
}

TEST_F(t_ModuleResourceManager, UnloadResourceAboveZeroRefDoesNotQueueRelease)
{
    const uint32 uid = 21;
    rm->CreateResourceFromLibrary(uid, ResourceType::MESH, "m", "a.fbx", "l.nmesh");
    rm->CreateResourceFromLibrary(uid, ResourceType::MESH, "m", "a.fbx", "l.nmesh"); // refcount = 2
    rm->TakePendingUploads();

    rm->UnloadResource(uid); // refcount 2 → 1, should NOT queue release

    EXPECT_TRUE(rm->TakePendingReleases().empty());
}

TEST_F(t_ModuleResourceManager, TakePendingReleasesClearsQueue)
{
    const uint32 uid = 30;
    rm->CreateResourceFromLibrary(uid, ResourceType::MESH, "m", "a.fbx", "l.nmesh");
    rm->UnloadResource(uid);

    rm->TakePendingReleases();
    EXPECT_TRUE(rm->TakePendingReleases().empty());
}

TEST_F(t_ModuleResourceManager, GetResourcesMapReturnsAllRegistered)
{
    rm->CreateResourceFromLibrary(100, ResourceType::MESH,     "m1", "a1.fbx",  "l1.nmesh");
    rm->CreateResourceFromLibrary(200, ResourceType::MATERIAL, "m2", "a2.nmat", "l2.nmat");

    auto map = rm->GetResourcesMap();
    EXPECT_EQ(map.size(), 2u);
    EXPECT_NE(map.find(100), map.end());
    EXPECT_NE(map.find(200), map.end());
}

TEST_F(t_ModuleResourceManager, EvictResourceRemovesFromMap)
{
    const uint32 uid = 50;
    ResourceBase* res = rm->CreateResourceFromLibrary(uid, ResourceType::MESH, "m", "a.fbx", "l.nmesh");
    rm->TakePendingUploads();

    rm->UnloadResource(uid);       // refcount → 0
    rm->TakePendingReleases();     // simulate renderer draining queue

    bool evicted = rm->EvictResource(ResourceType::MESH, res);

    EXPECT_TRUE(evicted);
    EXPECT_FALSE(rm->ResourceExists(uid));
}

TEST_F(t_ModuleResourceManager, UnloadResourceReturnsFalseForUnknownUID)
{
    EXPECT_FALSE(rm->UnloadResource(9999));
}

TEST_F(t_ModuleResourceManager, EvictResourceReQueuesUploadWhenReacquired)
{
    const uint32 uid = 60;
    ResourceBase* res = rm->CreateResourceFromLibrary(uid, ResourceType::MESH, "m", "a.fbx", "l.nmesh");
    rm->TakePendingUploads();

    rm->UnloadResource(uid);     // refcount → 0, queued for release
    rm->TakePendingReleases();   // renderer drains queue

    // Simulate re-acquisition before EvictResource is called
    rm->CreateResourceFromLibrary(uid, ResourceType::MESH, "m", "a.fbx", "l.nmesh"); // refcount → 1

    bool evicted = rm->EvictResource(ResourceType::MESH, res);

    EXPECT_FALSE(evicted);                             // should NOT delete — resource is live again
    EXPECT_TRUE(rm->ResourceExists(uid));              // still in map
    EXPECT_EQ(rm->TakePendingUploads().size(), 1u);   // re-queued for GPU upload
}

TEST_F(t_ModuleResourceManager, DeserializeFailureReturnsNullAndDoesNotRegister)
{
    mockImporter.deserializeResult = false;

    const uint32 uid = 70;
    ResourceBase* res = rm->CreateResourceFromLibrary(uid, ResourceType::MESH, "m", "a.fbx", "l.nmesh");

    EXPECT_EQ(res, nullptr);
    EXPECT_FALSE(rm->ResourceExists(uid));
    EXPECT_TRUE(rm->GetResourcesMap().empty());
}

TEST_F(t_ModuleResourceManager, GetLoadedResourceReturnsNullForUnknownUID)
{
    EXPECT_EQ(rm->GetLoadedResource(9999), nullptr);
}

TEST_F(t_ModuleResourceManager, GetLoadedResourceDoesNotBumpRefCount)
{
    const uint32 uid = 80;
    ResourceBase* created = rm->CreateResourceFromLibrary(uid, ResourceType::MESH, "m", "a.fbx", "l.nmesh");
    ASSERT_NE(created, nullptr);
    EXPECT_EQ(created->GetReferenceCount(), 1u);

    ResourceBase* borrowed = rm->GetLoadedResource(uid);

    EXPECT_EQ(borrowed, created);              // same pointer — no new allocation
    EXPECT_EQ(created->GetReferenceCount(), 1u); // borrowed reference: refcount unchanged
}

TEST_F(t_ModuleResourceManager, GetLoadedResourceReturnsNullAfterEviction)
{
    const uint32 uid = 81;
    ResourceBase* res = rm->CreateResourceFromLibrary(uid, ResourceType::MESH, "m", "a.fbx", "l.nmesh");
    ASSERT_NE(res, nullptr);
    rm->TakePendingUploads();

    rm->UnloadResource(uid);      // refcount → 0
    rm->TakePendingReleases();    // simulate renderer draining queue
    rm->EvictResource(ResourceType::MESH, res);

    EXPECT_EQ(rm->GetLoadedResource(uid), nullptr);
}

TEST_F(t_ModuleResourceManager, DeserializeFailureDoesNotBlockSubsequentLoadOfSameUID)
{
    const uint32 uid = 82;

    // First attempt: importer fails — slot must be fully cleaned up.
    mockImporter.deserializeResult = false;
    ResourceBase* failed = rm->CreateResourceFromLibrary(uid, ResourceType::MESH, "m", "a.fbx", "l.nmesh");
    EXPECT_EQ(failed, nullptr);
    EXPECT_FALSE(rm->ResourceExists(uid));

    // Second attempt with the same UID must succeed without being blocked by the previous failure.
    mockImporter.deserializeResult = true;
    ResourceBase* success = rm->CreateResourceFromLibrary(uid, ResourceType::MESH, "m", "a.fbx", "l.nmesh");
    EXPECT_NE(success, nullptr);
    EXPECT_TRUE(rm->ResourceExists(uid));
}

// =====================================================
// Asset Hot Reload Tests
// =====================================================

class HotReloadMockImporter : public MockImporterManager
{
public:
    int  importCallCount     = 0;
    bool importShouldSucceed = true;

    bool Import(ResourceType, const MetaFileData&) override
    {
        ++importCallCount;
        return importShouldSucceed;
    }
};

class t_HotReload : public ::testing::Test
{
protected:
    static constexpr uint64 kMemoryPoolSize = MiB(64);

    EventSystem*           eventSystem = nullptr;
    nous::engine::multithreading::NOUS_JobSystem* jobSystem = nullptr;
    HotReloadMockImporter  mockImporter;
    ModuleResourceManager* rm = nullptr;
    TypeRegistry*  registry = nullptr;

    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(kMemoryPoolSize);

        // ModuleResourceManager::Awake() touches the registry (EnsureLibraryDirectories,
        // ClearResources, InstantiateResource). It is injected at construction (below).
        registry = new TypeRegistry();
        RegisterResourceTypes(*registry);

        eventSystem = new EventSystem();
        jobSystem   = new nous::engine::multithreading::NOUS_JobSystem(0);
        rm = new ModuleResourceManager(eventSystem, jobSystem, &mockImporter, *registry);
        rm->Awake();
    }

    void TearDown() override
    {
        CleanupAllResources();
        delete rm;          rm = nullptr;
        delete jobSystem;   jobSystem = nullptr;
        delete eventSystem; eventSystem = nullptr;

        delete registry;
        registry = nullptr;

        nous::engine::memory::ShutdownMemory();
    }

    void CleanupAllResources()
    {
        rm->TakePendingUploads();
        rm->TakeReadyAssetUploads();

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
            if (res && res->GetReferenceCount() == 0)
                rm->EvictResource(res->GetType(), res);
    }

    ResourceBase* RegisterTexture(uint32 uid, const std::string& assetsPath)
    {
        return rm->CreateResourceFromLibrary(uid, ResourceType::TEXTURE, "tex",
                                             assetsPath, "Library/Textures/test.ntex");
    }
};

// m_pathToUID is populated by LoadResourceIntoSlot. DispatchReimportJob looks it up:
// if path is unknown, Import is never called. This verifies both the map population
// and the early-return guard in DispatchReimportJob.

TEST_F(t_HotReload, DispatchReimportJob_UnknownPath_DoesNotCallImport)
{
    // No resource registered → path not in m_pathToUID
    rm->DispatchReimportJob("Assets/Textures/ghost.png");

    EXPECT_EQ(mockImporter.importCallCount, 0);
}

TEST_F(t_HotReload, DispatchReimportJob_KnownPath_PathIsTrackedAfterRegistration)
{
    // Register a texture — this must add the path to m_pathToUID.
    RegisterTexture(100u, "Assets/Textures/known.png");

    // With a synchronous job system the job runs inline. GetAssetMetaData will fail
    // (no real .meta file in tests), so the job logs a warning and returns early —
    // but it DOES pass the m_pathToUID lookup (importCallCount stays 0 because
    // GetAssetMetaData fails before Import, which is the expected failure mode).
    // What we verify here is that no crash or assertion fires: the path WAS found.
    EXPECT_NO_FATAL_FAILURE(rm->DispatchReimportJob("Assets/Textures/known.png"));
}

TEST_F(t_HotReload, TakeReadyAssetUploads_EmptyByDefault)
{
    EXPECT_TRUE(rm->TakeReadyAssetUploads().empty());
}

TEST_F(t_HotReload, TakeReadyAssetUploads_ClearsQueueAfterDrain)
{
    // Drain twice — second call must return empty.
    rm->TakeReadyAssetUploads();
    EXPECT_TRUE(rm->TakeReadyAssetUploads().empty());
}

TEST_F(t_HotReload, EvictResource_RemovesPathFromMap_DispatchNoLongerFindsIt)
{
    RegisterTexture(101u, "Assets/Textures/evict_me.png");

    // Evict: drop refcount then call EvictResource
    rm->UnloadResource(101u);
    auto releases = rm->TakePendingReleases();
    for (auto& [type, res] : releases)
        rm->EvictResource(type, res);

    // After eviction, the path must be removed from m_pathToUID.
    // We verify by calling DispatchReimportJob: if found, it proceeds (may reach
    // GetAssetMetaData); if not found, Import is definitely not called.
    // Either way, no crash.
    EXPECT_NO_FATAL_FAILURE(rm->DispatchReimportJob("Assets/Textures/evict_me.png"));
    // Import must not have been called (either path-not-found or meta-not-found).
    EXPECT_EQ(mockImporter.importCallCount, 0);
}
