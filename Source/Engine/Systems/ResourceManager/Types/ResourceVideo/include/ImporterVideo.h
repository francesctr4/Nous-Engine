#pragma once

#include "Engine/Systems/ResourceManager/Core/ImporterManager/include/IImporter.h"
#include "Engine/EngineExport.h"

struct NOUS_ENGINE_API ImporterVideo : IResourceImporter
{
    bool Import(const MetaFileData& metaFileData) override;
    bool Save(const MetaFileData& metaFileData, ResourceBase*& inResource) override;
    bool Deserialize(const std::string& libraryPath, ResourceBase* resource) override;
    void Evict(ResourceBase* resource) override;
    bool Upload(ResourceBase* resource, IGPUResourceFactory* gpu) override;
    void Release(ResourceBase* resource, IGPUResourceFactory* gpu) override;
};
