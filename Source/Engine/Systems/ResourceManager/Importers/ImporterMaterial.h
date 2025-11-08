#ifndef IMPORTERMATERIAL_H
#define IMPORTERMATERIAL_H

#include <Engine/Systems/ResourceManager/Importers/Importer.inl>

struct ImporterMaterial : Importer
{
    bool Import(const MetaFileData& metaFileData) override;
    bool Save(const MetaFileData& metaFileData, Resource*& inResource) override;
    bool Load(const std::string& libraryPath, Resource* outResource) override;
    bool Unload(Resource* inResource) override;
};

#endif // IMPORTERMATERIAL_H