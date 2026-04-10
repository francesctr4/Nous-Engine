#include <Engine/Systems/ResourceManager/Importer/ImporterManager.h>

#include <Engine/Systems/ResourceManager/Importer/Importer.inl>
#include <Engine/Systems/ResourceManager/Resource/Resource.h>

#include "Engine/Systems/ResourceManager/Importer/ImporterMaterial/include/ImporterMaterial.h"
#include "Engine/Systems/ResourceManager/Importer/ImporterTexture/include/ImporterTexture.h"
#include "Engine/Systems/ResourceManager/Importer/ImporterMesh/include/ImporterMesh.h"
#include "Engine/Systems/ResourceManager/Importer/ImporterShader/include/ImporterShader.h"

#include <array>

// Make sure to match the array index with the resource type enum index
const std::array<std::unique_ptr<Importer>, c_NUM_IMPORTERS> ImporterManager::importers =
{
    std::make_unique<ImporterMesh>(),
    std::make_unique<ImporterMaterial>(),
    std::make_unique<ImporterTexture>(),
    std::make_unique<ImporterShader>()
};

void ImporterManager::Init(ModuleResourceManager* resourceManager)
{
    for (auto& importer : importers)
        importer->mResourceManager = resourceManager;
}

bool ImporterManager::Import(const ResourceType& type, const MetaFileData& metaFileData)
{
    const int16 idx = Resource::GetIndexFromType(type);
    if (idx < 0 || static_cast<size_t>(idx) >= importers.size()) return false;
    return importers[idx]->Import(metaFileData);
}

bool ImporterManager::Save(const ResourceType& type, const MetaFileData& metaFileData, Resource*& inResource)
{
    const int16 idx = Resource::GetIndexFromType(type);
    if (idx < 0 || static_cast<size_t>(idx) >= importers.size()) return false;
    return importers[idx]->Save(metaFileData, inResource);
}

bool ImporterManager::Deserialize(const ResourceType& type, const std::string& libraryPath, Resource* resource)
{
    const int16 idx = Resource::GetIndexFromType(type);
    if (idx < 0 || static_cast<size_t>(idx) >= importers.size()) return false;
    return importers[idx]->Deserialize(libraryPath, resource);
}

void ImporterManager::Evict(const ResourceType& type, Resource* resource)
{
    const int16 idx = Resource::GetIndexFromType(type);
    if (idx < 0 || static_cast<size_t>(idx) >= importers.size()) return;
    importers[idx]->Evict(resource);
}

bool ImporterManager::Upload(const ResourceType& type, Resource* resource, IGPUResourceFactory* gpu)
{
    const int16 idx = Resource::GetIndexFromType(type);
    if (idx < 0 || static_cast<size_t>(idx) >= importers.size()) return false;
    return importers[idx]->Upload(resource, gpu);
}

void ImporterManager::Release(const ResourceType& type, Resource* resource, IGPUResourceFactory* gpu)
{
    const int16 idx = Resource::GetIndexFromType(type);
    if (idx < 0 || static_cast<size_t>(idx) >= importers.size()) return;
    importers[idx]->Release(resource, gpu);
}
