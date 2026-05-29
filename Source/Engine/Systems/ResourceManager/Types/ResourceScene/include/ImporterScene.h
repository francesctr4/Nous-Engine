#pragma once

#include "Engine/EngineExport.h"
#include "Engine/Systems/ResourceManager/Core/ImporterManager/include/IImporter.h"

// Scene importer — scenes have no runtime resource object (no `ResourceScene`
// class) and no GPU residency. The only work to do at import time is copy the
// source `.nous` from Assets/ to its UID-keyed Library/Scenes/ destination,
// matching how every other library asset is laid out. Deserialize/Upload/
// Release/Evict are no-ops; scene loading goes through ModuleScene, not the
// generic Resource lifecycle.
class ImporterScene final : public IImporter
{
public:
    NOUS_ENGINE_API bool Import(const MetaFileData& metaFileData) override;
    NOUS_ENGINE_API bool Save(const MetaFileData& metaFileData, Resource*& inResource) override;
    NOUS_ENGINE_API bool Deserialize(const std::string& libraryPath, Resource* resource) override;
    NOUS_ENGINE_API void Evict(Resource* resource) override;
    NOUS_ENGINE_API bool Upload(Resource* resource, IGPUResourceFactory* gpu) override;
    NOUS_ENGINE_API void Release(Resource* resource, IGPUResourceFactory* gpu) override;
};
