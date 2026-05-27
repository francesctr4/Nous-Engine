#pragma once

#include "Engine/EngineExport.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/Resource/Resource.h"
#include <string>
#include <string_view>
#include <vector>

class IImporterManager;
struct MetaFileData;
namespace nous::engine::multithreading { class NOUS_JobSystem; }

class ResourceImportPipeline
{
public:
    NOUS_ENGINE_API ResourceImportPipeline(IImporterManager* importerManager,
                                           nous::engine::multithreading::NOUS_JobSystem* jobSystem = nullptr);

    // Public import entry points — called by ModuleResourceManager delegators
    // and by external consumers that formerly called ModuleResourceManager directly.
    NOUS_ENGINE_API bool ImportFile(const std::string& path);
    NOUS_ENGINE_API bool ImportDirectory(const std::string& directory);
    NOUS_ENGINE_API void ScanAndImportAssets(bool parallelImports = true);

    // Reads the .meta sidecar for assetsPath and fills outData.
    // Returns false if the meta file is missing or malformed.
    // Called from Awake() via ModuleResourceManager; also called by external consumers.
    static NOUS_ENGINE_API bool GetAssetMetaData(const std::string& assetsPath, MetaFileData& outData);

    // Ensures Library/ subdirectories exist. Idempotent, safe to call repeatedly.
    // Public because ModuleResourceManager::Awake() calls it before ScanAndImportAssets.
    NOUS_ENGINE_API bool EnsureLibraryDirectories();

    // Deletes all binary files inside Library/ subdirectories without touching .meta
    // sidecars in Assets/. Call before ScanAndImportAssets() to force a full reimport.
    NOUS_ENGINE_API void ClearLibraryFiles();

private:
    static bool CreateMetaFile(const std::string& metaFilePath, const MetaFileData& inFileData);
    static bool ReadMetaFile(const std::string& metaFilePath, MetaFileData& outFileData);

    // Writes Library/Shaders/shader_manifest.json from the .meta files of the built-in
    // shaders. Called at the tail of ScanAndImportAssets so the manifest is always in
    // sync with the library after any import pass — initial startup, RegenerateLibrary,
    // or a manual Library/ nuke followed by a relaunch. GAME mode requires the manifest
    // to locate built-in shaders without reading .meta files at runtime.
    static void WriteShaderManifest();

    bool ImportFileFromExternal(const std::string& path, ResourceType resourceType,
                                const std::string& fileName, const std::string& extension);
    bool ImportFileFromAssets(const std::string& relativePath, ResourceType resourceType,
                              const std::string& fileName, const std::string& extension) const;
    bool ImportCase1_NewAsset(std::string_view relativePath, const std::string& metaFilePath,
                              ResourceType resourceType, std::string_view fileName) const;
    bool ImportCase2_MissingLibrary(const MetaFileData& metaFileData) const;
    bool ImportCase3_TimestampCheck(const MetaFileData& metaFileData) const;

    IImporterManager* m_importerManager = nullptr;
    nous::engine::multithreading::NOUS_JobSystem* m_jobSystem = nullptr;

    // Phase-1 scan: walks directory, handles meta file creation, and collects
    // MetaFileData for every file that actually needs import work. No imports are
    // performed — safe to call from the main thread without holding any locks.
    void CollectPendingImports(const std::string& directory,
                               std::vector<MetaFileData>& outPending) const;
};
