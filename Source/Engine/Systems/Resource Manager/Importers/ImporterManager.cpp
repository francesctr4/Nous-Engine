#include <Engine/Systems/Resource Manager/Importers/ImporterManager.h>

#include <Engine/Systems/Resource Manager/Importers/Importer.inl>
#include <Engine/Systems/Resource Manager/Resource Types/Resource.h>

#include <Engine/Systems/Resource Manager/Importers/ImporterMaterial.h>
#include <Engine/Systems/Resource Manager/Importers/ImporterTexture.h>
#include <Engine/Systems/Resource Manager/Importers/ImporterMesh.h>

#include <array>

// Make sure to match the array index with the resource type enum index
const std::array<std::unique_ptr<Importer>, c_NUM_IMPORTERS> ImporterManager::importers =
{
    std::make_unique<ImporterMesh>(),
    std::make_unique<ImporterMaterial>(),
    std::make_unique<ImporterTexture>()
};

bool ImporterManager::Import(const ResourceType& type, const MetaFileData& metaFileData)
{
    return importers[Resource::GetIndexFromType(type)]->Import(metaFileData);
}

bool ImporterManager::Save(const ResourceType& type, const MetaFileData& metaFileData, Resource*& inResource)
{
    return importers[Resource::GetIndexFromType(type)]->Save(metaFileData, inResource);
}

bool ImporterManager::Load(const ResourceType& type, const std::string& libraryPath, Resource* outResource)
{
    return importers[Resource::GetIndexFromType(type)]->Load(libraryPath, outResource);
}

bool ImporterManager::Unload(const ResourceType& type, Resource* inResource)
{
    return importers[Resource::GetIndexFromType(type)]->Unload(inResource);
}
