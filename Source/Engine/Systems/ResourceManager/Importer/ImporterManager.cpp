#include <Engine/Systems/ResourceManager/Importer/ImporterManager.h>

#include <Engine/Systems/ResourceManager/Importer/Importer.inl>
#include <Engine/Systems/ResourceManager/Resource/Resource.h>

#include "Engine/Systems/ResourceManager/Importer/ImporterMaterial/include/ImporterMaterial.h"
#include "Engine/Systems/ResourceManager/Importer/ImporterTexture/include/ImporterTexture.h"
#include "Engine/Systems/ResourceManager/Importer/ImporterMesh/include/ImporterMesh.h"
#include "Engine/Systems/ResourceManager/Importer/ImporterShader/include/ImporterShader.h"
#include "Engine/Systems/ResourceManager/Importer/ImporterAudio/include/ImporterAudio.h"

#include <unordered_map>

static std::unordered_map<ResourceType, std::unique_ptr<Importer>> MakeImporters()
{
    std::unordered_map<ResourceType, std::unique_ptr<Importer>> map;
    map.emplace(ResourceType::MESH,     std::make_unique<ImporterMesh>());
    map.emplace(ResourceType::MATERIAL, std::make_unique<ImporterMaterial>());
    map.emplace(ResourceType::TEXTURE,  std::make_unique<ImporterTexture>());
    map.emplace(ResourceType::SHADER,   std::make_unique<ImporterShader>());
    map.emplace(ResourceType::AUDIO,   std::make_unique<ImporterAudio>());
    return map;
}

const std::unordered_map<ResourceType, std::unique_ptr<Importer>> ImporterManager::importers = MakeImporters();

void ImporterManager::Init(ModuleResourceManager* resourceManager)
{
    for (auto& [type, importer] : importers)
        importer->mResourceManager = resourceManager;
}

bool ImporterManager::Import(ResourceType type, const MetaFileData& metaFileData)
{
    const auto it = importers.find(type);
    if (it == importers.end()) return false;
    return it->second->Import(metaFileData);
}

bool ImporterManager::Save(ResourceType type, const MetaFileData& metaFileData, Resource*& inResource)
{
    const auto it = importers.find(type);
    if (it == importers.end()) return false;
    return it->second->Save(metaFileData, inResource);
}

bool ImporterManager::Deserialize(ResourceType type, const std::string& libraryPath, Resource* resource)
{
    const auto it = importers.find(type);
    if (it == importers.end()) return false;
    return it->second->Deserialize(libraryPath, resource);
}

void ImporterManager::Evict(ResourceType type, Resource* resource)
{
    const auto it = importers.find(type);
    if (it == importers.end()) return;
    it->second->Evict(resource);
}

bool ImporterManager::Upload(ResourceType type, Resource* resource, IGPUResourceFactory* gpu)
{
    const auto it = importers.find(type);
    if (it == importers.end()) return false;
    return it->second->Upload(resource, gpu);
}

void ImporterManager::Release(ResourceType type, Resource* resource, IGPUResourceFactory* gpu)
{
    const auto it = importers.find(type);
    if (it == importers.end()) return;
    it->second->Release(resource, gpu);
}
