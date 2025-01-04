#include "ImporterManager.h"

#include "Importer.inl"
#include "Resource.h"

#include "ImporterMaterial.h"
#include "ImporterTexture.h"
#include "Importermesh.h"

// Define the static member
std::unordered_map<ResourceType, std::unique_ptr<Importer>> ImporterManager::importers;

void ImporterManager::Initialize()
{
    if (importers.empty())
    {
        importers[ResourceType::MESH] = std::make_unique<ImporterMesh>();
        importers[ResourceType::MATERIAL] = std::make_unique<ImporterMaterial>();
        importers[ResourceType::TEXTURE] = std::make_unique<ImporterTexture>();
    }
}

void ImporterManager::Shutdown()
{
    importers.clear();
}

bool ImporterManager::Import(ResourceType type, const std::string& assetsPath)
{
    auto it = importers.find(type);
    if (it != importers.end())
    {
        return it->second->Import(assetsPath);
    }
    return false; // Importer not found
}

bool ImporterManager::Save(ResourceType type, const std::string& libraryPath, const Resource* inResource)
{
    auto it = importers.find(type);
    if (it != importers.end())
    {
        return it->second->Save(libraryPath, inResource);
    }
    return false; // Importer not found
}

bool ImporterManager::Load(ResourceType type, const std::string& libraryPath, Resource* outResource)
{
    auto it = importers.find(type);
    if (it != importers.end())
    {
        return it->second->Load(libraryPath, outResource);
    }
    return false; // Importer not found
}
