#pragma once

#include "Engine/Systems/ResourceManager/Core/ImporterManager/include/IImporter.h"
#include "Engine/EngineExport.h"

struct NOUS_ENGINE_API ImporterAudio : IImporter
{
    bool Import(const MetaFileData& metaFileData) override;
    bool Save(const MetaFileData& metaFileData, Resource*& inResource) override;
    bool Deserialize(const std::string& libraryPath, Resource* resource) override;
    void Evict(Resource* resource) override;
    bool Upload(Resource* resource, IGPUResourceFactory* gpu) override;
    void Release(Resource* resource, IGPUResourceFactory* gpu) override;
};
