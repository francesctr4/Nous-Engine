#pragma once

#include "Engine/EngineExport.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include <string>
#include <string_view>

class IImporterManager;
struct MetaFileData;

class ResourceImportPipeline
{
public:
    NOUS_ENGINE_API explicit ResourceImportPipeline(IImporterManager* importerManager);

    // Public import entry points — called by ModuleResourceManager delegators
    // and by external consumers that formerly called ModuleResourceManager directly.
    NOUS_ENGINE_API bool ImportFile(const std::string& path);
    NOUS_ENGINE_API bool ImportDirectory(const std::string& directory);
    NOUS_ENGINE_API void ScanAndImportAssets();

    // Reads the .meta sidecar for assetsPath and fills outData.
    // Returns false if the meta file is missing or malformed.
    // Called from Awake() via ModuleResourceManager; also called by external consumers.
    static NOUS_ENGINE_API bool GetAssetMetaData(const std::string& assetsPath, MetaFileData& outData);

    // Ensures Library/ subdirectories exist. Idempotent, safe to call repeatedly.
    // Public because ModuleResourceManager::Awake() calls it before ScanAndImportAssets.
    NOUS_ENGINE_API bool EnsureLibraryDirectories();

private:
    static bool CreateMetaFile(const std::string& metaFilePath, const MetaFileData& inFileData);
    static bool ReadMetaFile(const std::string& metaFilePath, MetaFileData& outFileData);

    bool ImportFileFromExternal(const std::string& path, ResourceType resourceType,
                                const std::string& fileName, const std::string& extension);
    bool ImportFileFromAssets(const std::string& relativePath, ResourceType resourceType,
                              const std::string& fileName, const std::string& extension) const;
    bool ImportCase1_NewAsset(std::string_view relativePath, const std::string& metaFilePath,
                              ResourceType resourceType, std::string_view fileName) const;
    bool ImportCase2_MissingLibrary(const MetaFileData& metaFileData) const;
    bool ImportCase3_TimestampCheck(const MetaFileData& metaFileData) const;

    IImporterManager* m_importerManager = nullptr;
};
