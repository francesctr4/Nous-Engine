#pragma once

#include "Globals.h"

class Resource;
struct MetaFileData;

struct Importer
{
    virtual ~Importer() = default;

    virtual bool Import(const MetaFileData& metaFileData) = 0;
    virtual bool Save(const MetaFileData& metaFileData, Resource*& inResource) = 0;
    virtual bool Load(const std::string& libraryPath, Resource* outResource) = 0;
    virtual bool Unload(Resource* inResource) = 0;
};