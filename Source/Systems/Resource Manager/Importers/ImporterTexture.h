#ifndef IMPORTERTEXTURE_H
#define IMPORTERTEXTURE_H

#include "Systems/Resource Manager/Importers/Importer.inl"

struct ImporterTexture : Importer
{
    bool Import(const MetaFileData& metaFileData) override;
    bool Save(const MetaFileData& metaFileData, Resource*& inResource) override;
    bool Load(const std::string& libraryPath, Resource* outResource) override;
    bool Unload(Resource* inResource) override;
};

#endif // IMPORTERTEXTURE_H