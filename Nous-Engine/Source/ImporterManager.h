#pragma once

#include "Globals.h"

class Resource;
enum class ResourceType;

struct Importer;

class ImporterManager
{
public:

    ImporterManager() = delete; // Prevent instantiation
    ~ImporterManager() = delete;

    static void Initialize();
    static void Shutdown();

    static bool Import(ResourceType type, const std::string& assetsPath);
    static bool Save(ResourceType type, const std::string& libraryPath, const Resource* inResource);
    static bool Load(ResourceType type, const std::string& libraryPath, Resource* outResource);

private:

    static std::unordered_map<ResourceType, std::unique_ptr<Importer>> importers;

};