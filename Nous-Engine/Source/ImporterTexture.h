#pragma once

#include "Importer.inl"

struct ImporterTexture : Importer
{
    bool Import(const MetaFileData& metaFileData) override;
    bool Save(const MetaFileData& metaFileData, Resource*& inResource) override;
    bool Load(const MetaFileData& metaFileData, Resource* outResource) override;
    bool Unload(Resource*& inResource) override;
};