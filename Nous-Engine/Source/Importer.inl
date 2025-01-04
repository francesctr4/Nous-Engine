#pragma once

#include "Globals.h"

class Resource;
struct MetaFileData;

struct Importer
{
    virtual ~Importer() = default;

    virtual bool Import(const MetaFileData& metaFileData) = 0;
    virtual bool Save(const MetaFileData& metaFileData, const Resource* inResource) = 0;
    virtual bool Load(const MetaFileData& metaFileData, Resource* outResource) = 0;
};