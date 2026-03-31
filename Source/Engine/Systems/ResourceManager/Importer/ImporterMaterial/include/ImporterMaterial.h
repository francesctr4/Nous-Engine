#ifndef IMPORTERMATERIAL_H
#define IMPORTERMATERIAL_H

#include "Engine/Systems/ResourceManager/Importer/Importer.inl"
#include "Engine/EngineExport.h"

class ResourceMaterial;

struct ImporterMaterial : Importer
{
    bool Import(const MetaFileData& metaFileData) override;
    bool Save(const MetaFileData& metaFileData, Resource*& inResource) override;
    bool Deserialize(const std::string& libraryPath, Resource* resource) override;
    void Evict(Resource* resource) override;
    bool Upload(Resource* resource, IGPUResourceFactory* gpu) override;
    void Release(Resource* resource, IGPUResourceFactory* gpu) override;

    static NOUS_ENGINE_API bool SaveMaterialToAssets(ResourceMaterial* material);
};

#endif // IMPORTERMATERIAL_H