#pragma once

#include "Globals.h"

class Resource;
enum class ResourceType;

struct Importer;
struct MetaFileData;

constexpr int16 c_NUM_IMPORTERS = 3;

class ImporterManager
{
public:

    static bool Import(const ResourceType& type, const MetaFileData& metaFileData);
    static bool Save(const ResourceType& type, const MetaFileData& metaFileData, Resource*& inResource);
    static bool Load(const ResourceType& type, const MetaFileData& metaFileData, Resource* outResource);
    static bool Unload(const ResourceType& type, Resource*& inResource);

private:

    static const std::array<std::unique_ptr<Importer>, c_NUM_IMPORTERS> importers;

};