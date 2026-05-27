#ifndef IMPORTERTEXTURE_H
#define IMPORTERTEXTURE_H

#include "Engine/Systems/ResourceManager/ImporterManager/Importer.inl"
#include "Engine/EngineExport.h"

struct NOUS_ENGINE_API ImporterTexture : Importer
{
    bool Import(const MetaFileData& metaFileData) override;
    bool Save(const MetaFileData& metaFileData, Resource*& inResource) override;
    bool Deserialize(const std::string& libraryPath, Resource* resource) override;
    void Evict(Resource* resource) override;
    bool Upload(Resource* resource, IGPUResourceFactory* gpu) override;
    void Release(Resource* resource, IGPUResourceFactory* gpu) override;
};

#endif // IMPORTERTEXTURE_H