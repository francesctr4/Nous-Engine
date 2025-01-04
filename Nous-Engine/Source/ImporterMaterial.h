#pragma once

#include "Importer.inl"

struct ImporterMaterial : Importer
{
    bool Import(const MetaFileData& metaFileData) override;
    bool Save(const MetaFileData& metaFileData, const Resource* inResource) override;
    bool Load(const MetaFileData& metaFileData, Resource* outResource) override;
};