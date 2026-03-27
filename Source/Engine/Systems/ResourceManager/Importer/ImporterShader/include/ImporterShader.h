#ifndef NOUS_ENGINE_IMPORTER_SHADER_H
#define NOUS_ENGINE_IMPORTER_SHADER_H

#include "Engine/Systems/ResourceManager/Importer/Importer.inl"

struct ImporterShader : Importer
{
    bool Import(const MetaFileData& metaFileData) override;
    bool Save(const MetaFileData& metaFileData, Resource*& inResource) override;
    bool Deserialize(const std::string& libraryPath, Resource* resource) override;
    void Evict(Resource* resource) override;
    bool Upload(Resource* resource, IGPUResourceFactory* gpu) override;
    void Release(Resource* resource, IGPUResourceFactory* gpu) override;
};

#endif //NOUS_ENGINE_IMPORTER_SHADER_H