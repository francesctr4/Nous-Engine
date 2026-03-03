#ifndef NOUS_ENGINE_IMPORTER_SHADER_H
#define NOUS_ENGINE_IMPORTER_SHADER_H

#include "Engine/Systems/ResourceManager/Importer/Importer.inl"

struct ImporterShader : Importer
{
    bool Import(const MetaFileData& metaFileData) override;
    bool Save(const MetaFileData& metaFileData, Resource*& inResource) override;
    bool Load(const std::string& libraryPath, Resource* outResource) override;
    bool Unload(Resource* inResource) override;
};

#endif //NOUS_ENGINE_IMPORTER_SHADER_H