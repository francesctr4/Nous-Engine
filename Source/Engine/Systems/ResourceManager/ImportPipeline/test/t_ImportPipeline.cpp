#include <gtest/gtest.h>

#include "Engine/Systems/ResourceManager/ImportPipeline/include/ImportPipeline.h"
#include "Engine/Systems/ResourceManager/ImporterManager/IImporterManager.h"
#include "Engine/Systems/ResourceManager/Resource/MetaFileData.inl"
#include "Engine/Systems/ResourceManager/TypeRegistry/TypeRegistry.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Globals.h"

#include <chrono>
#include <filesystem>
#include <fstream>

// =====================================================
// Mock
// =====================================================

class MockImporterManager : public IImporterManager
{
public:
    int importCallCount = 0;

    void Init(ModuleResourceManager*) override                                     {}
    bool Import(ResourceType, const MetaFileData&) override                        { ++importCallCount; return true; }
    bool Deserialize(ResourceType, const std::string&, Resource*) override         { return true; }
    void Evict(ResourceType, Resource*) override                                   {}
    bool Upload(ResourceType, Resource*, IGPUResourceFactory*) override            { return true; }
    void Release(ResourceType, Resource*, IGPUResourceFactory*) override           {}
};

// =====================================================
// Fixture
// =====================================================

class t_ImportPipeline : public ::testing::Test
{
protected:
    static constexpr uint64 kMemoryPoolSize = MiB(32);

    MockImporterManager     mockImporter;
    ImportPipeline* pipeline   = nullptr;
    TypeRegistry*   registry   = nullptr;
    std::filesystem::path   tempDir;
    std::filesystem::path   savedCwd;

    void SetUp() override
    {
        nous::engine::memory::InitializeMemory(kMemoryPoolSize);

        // ImportPipeline reads descriptors (libraryFolder, libExtPolicy)
        // from the global registry. Application sets this up in production; the
        // test harness must do it manually.
        registry = new TypeRegistry();
        RegisterResourceTypes(*registry);
        SetTypeRegistry(registry);

        pipeline = new ImportPipeline(&mockImporter);

        // Give each test an isolated sandbox. ImportPipeline works entirely
        // with relative paths from the process working directory, so we chdir into
        // a fresh temp directory that is cleaned up in TearDown.
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        tempDir = std::filesystem::temp_directory_path()
                / ("nous_pip_" + std::string(info->test_suite_name()) + "_" + info->name());
        std::filesystem::create_directories(tempDir);
        savedCwd = std::filesystem::current_path();
        std::filesystem::current_path(tempDir);
    }

    void TearDown() override
    {
        std::filesystem::current_path(savedCwd);
        std::filesystem::remove_all(tempDir);

        delete pipeline;
        pipeline = nullptr;

        SetTypeRegistry(nullptr);
        delete registry;
        registry = nullptr;

        nous::engine::memory::ShutdownMemory();
    }

    // Creates a file (and all parent directories) with stub content.
    static void CreateDummyFile(const std::filesystem::path& path)
    {
        std::filesystem::create_directories(path.parent_path());
        std::ofstream(path) << "dummy";
    }
};

// =====================================================
// EnsureLibraryDirectories
// =====================================================

TEST_F(t_ImportPipeline, EnsureLibraryDirectoriesCreatesAllSubdirs)
{
    EXPECT_TRUE(pipeline->EnsureLibraryDirectories());

    EXPECT_TRUE(std::filesystem::is_directory("Library"));
    EXPECT_TRUE(std::filesystem::is_directory("Library/Shaders"));
    EXPECT_TRUE(std::filesystem::is_directory("Library/Meshes"));
    EXPECT_TRUE(std::filesystem::is_directory("Library/Materials"));
    EXPECT_TRUE(std::filesystem::is_directory("Library/Textures"));
    EXPECT_TRUE(std::filesystem::is_directory("Library/Scenes"));
}

TEST_F(t_ImportPipeline, EnsureLibraryDirectoriesIsIdempotent)
{
    EXPECT_TRUE(pipeline->EnsureLibraryDirectories());
    EXPECT_TRUE(pipeline->EnsureLibraryDirectories()); // second call must not fail
}

// =====================================================
// ImportFile — basic guards
// =====================================================

TEST_F(t_ImportPipeline, ImportFileReturnsFalseForMissingFile)
{
    EXPECT_FALSE(pipeline->ImportFile("Assets/Textures/nonexistent.png"));
    EXPECT_EQ(mockImporter.importCallCount, 0);
}

TEST_F(t_ImportPipeline, ImportFileReturnsFalseForUnknownExtension)
{
    pipeline->EnsureLibraryDirectories();
    CreateDummyFile("Assets/Textures/file.xyz");

    EXPECT_FALSE(pipeline->ImportFile("Assets/Textures/file.xyz"));
    EXPECT_EQ(mockImporter.importCallCount, 0);
    EXPECT_FALSE(std::filesystem::exists("Assets/Textures/file.xyz.meta"));
}

// =====================================================
// ImportFile — Case 1: new asset (no .meta yet)
// =====================================================

TEST_F(t_ImportPipeline, ImportFileCase1CreatesMeta)
{
    pipeline->EnsureLibraryDirectories();
    CreateDummyFile("Assets/Textures/test.png");

    EXPECT_TRUE(pipeline->ImportFile("Assets/Textures/test.png"));
    EXPECT_TRUE(std::filesystem::exists("Assets/Textures/test.png.meta"));
}

TEST_F(t_ImportPipeline, ImportFileCase1CallsImportOnce)
{
    pipeline->EnsureLibraryDirectories();
    CreateDummyFile("Assets/Textures/test.png");

    pipeline->ImportFile("Assets/Textures/test.png");

    EXPECT_EQ(mockImporter.importCallCount, 1);
}

TEST_F(t_ImportPipeline, ImportFileCase1MetaRoundTrip)
{
    pipeline->EnsureLibraryDirectories();
    CreateDummyFile("Assets/Textures/test.png");
    pipeline->ImportFile("Assets/Textures/test.png");

    MetaFileData meta;
    EXPECT_TRUE(ImportPipeline::GetAssetMetaData("Assets/Textures/test.png", meta));
    EXPECT_EQ(meta.resourceType, ResourceType::TEXTURE);
    EXPECT_EQ(meta.name, "test");
    EXPECT_NE(meta.uid, 0u);
    EXPECT_FALSE(meta.libraryPath.empty());
}

// =====================================================
// ImportFile — Case 2: .meta exists, library file missing
// =====================================================

TEST_F(t_ImportPipeline, ImportFileCase2MissingLibraryTriggersReImport)
{
    pipeline->EnsureLibraryDirectories();
    CreateDummyFile("Assets/Textures/test.png");

    // Case 1: creates .meta. Mock importer never writes the library binary,
    // so the library path in the meta does not exist on disk.
    pipeline->ImportFile("Assets/Textures/test.png");
    mockImporter.importCallCount = 0; // reset

    // Second call: .meta found, library absent → Case 2 must re-import.
    EXPECT_TRUE(pipeline->ImportFile("Assets/Textures/test.png"));
    EXPECT_EQ(mockImporter.importCallCount, 1);
}

// =====================================================
// ImportFile — Case 3: timestamp check
// =====================================================

TEST_F(t_ImportPipeline, ImportFileCase3UpToDateLibrarySkipsImport)
{
    pipeline->EnsureLibraryDirectories();
    CreateDummyFile("Assets/Textures/test.png");
    pipeline->ImportFile("Assets/Textures/test.png"); // Case 1: creates .meta

    MetaFileData meta;
    ImportPipeline::GetAssetMetaData("Assets/Textures/test.png", meta);

    // Create library file and stamp it with a future mtime so it looks newer than the asset.
    CreateDummyFile(meta.libraryPath);
    const auto future = std::filesystem::file_time_type::clock::now() + std::chrono::hours(1);
    std::filesystem::last_write_time(meta.libraryPath, future);

    mockImporter.importCallCount = 0;
    EXPECT_TRUE(pipeline->ImportFile("Assets/Textures/test.png")); // Case 3: up-to-date
    EXPECT_EQ(mockImporter.importCallCount, 0);
}

TEST_F(t_ImportPipeline, ImportFileCase3StaleLibraryTriggersReImport)
{
    pipeline->EnsureLibraryDirectories();
    CreateDummyFile("Assets/Textures/test.png");
    pipeline->ImportFile("Assets/Textures/test.png"); // Case 1: creates .meta

    MetaFileData meta;
    ImportPipeline::GetAssetMetaData("Assets/Textures/test.png", meta);

    // Create library file and stamp it with a past mtime so it looks older than the asset.
    CreateDummyFile(meta.libraryPath);
    const auto past = std::filesystem::file_time_type::clock::now() - std::chrono::hours(1);
    std::filesystem::last_write_time(meta.libraryPath, past);

    mockImporter.importCallCount = 0;
    EXPECT_TRUE(pipeline->ImportFile("Assets/Textures/test.png")); // Case 3: stale → re-import
    EXPECT_EQ(mockImporter.importCallCount, 1);
}

// =====================================================
// GetAssetMetaData
// =====================================================

TEST_F(t_ImportPipeline, GetAssetMetaDataReturnsFalseForMissingMeta)
{
    MetaFileData out;
    EXPECT_FALSE(ImportPipeline::GetAssetMetaData("Assets/Textures/ghost.png", out));
}

// =====================================================
// ImportDirectory
// =====================================================

TEST_F(t_ImportPipeline, ImportDirectoryReturnsFalseForMissingDirectory)
{
    EXPECT_FALSE(pipeline->ImportDirectory("Assets/DoesNotExist"));
}

TEST_F(t_ImportPipeline, ImportDirectoryCallsImportForEachKnownFile)
{
    pipeline->EnsureLibraryDirectories();
    CreateDummyFile("Assets/Textures/a.png");
    CreateDummyFile("Assets/Textures/b.png");
    CreateDummyFile("Assets/Textures/ignore.xyz"); // unknown extension — skipped

    EXPECT_TRUE(pipeline->ImportDirectory("Assets"));
    EXPECT_EQ(mockImporter.importCallCount, 2);
}

// =====================================================
// Meta path — adjacent to asset (not type-specific folder)
// =====================================================

TEST_F(t_ImportPipeline, ImportFileCreatesMetaAdjacentToAsset)
{
    pipeline->EnsureLibraryDirectories();
    CreateDummyFile("Assets/CustomFolder/SubFolder/test.png");

    EXPECT_TRUE(pipeline->ImportFile("Assets/CustomFolder/SubFolder/test.png"));
    EXPECT_TRUE(std::filesystem::exists("Assets/CustomFolder/SubFolder/test.png.meta"));
    EXPECT_FALSE(std::filesystem::exists("Assets/Textures/test.png.meta"));
}

TEST_F(t_ImportPipeline, ScanCreatesMetaAdjacentToAsset)
{
    pipeline->EnsureLibraryDirectories();
    CreateDummyFile("Assets/CustomFolder/SubFolder/model.fbx");

    pipeline->ScanAndImportAssets();

    EXPECT_TRUE(std::filesystem::exists("Assets/CustomFolder/SubFolder/model.fbx.meta"));
    EXPECT_FALSE(std::filesystem::exists("Assets/Meshes/model.fbx.meta"));
}
