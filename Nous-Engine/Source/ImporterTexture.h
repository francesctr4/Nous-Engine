#pragma once

#include "Importer.inl"

struct ImporterTexture : Importer
{
    bool Import(const std::string& assetsPath) override;
    bool Save(const std::string& libraryPath, const Resource* inResource) override;
    bool Load(const std::string& libraryPath, Resource* outResource) override;
};