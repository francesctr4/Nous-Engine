#include <Engine/Systems/ResourceManager/Importer/ImporterManager.h>

#include <Engine/Systems/ResourceManager/Importer/Importer.inl>
#include <Engine/Systems/ResourceManager/Resource/Resource.h>
#include "Engine/Systems/ResourceManager/ResourceTypeRegistry/ResourceTypeRegistry.h"

namespace
{
    Importer* ImporterFor(ResourceType type)
    {
        const ResourceTypeDescriptor* d = GetResourceTypeRegistry().Get(type);
        return d ? d->importer.get() : nullptr;
    }
}

void ImporterManager::Init(ModuleResourceManager* resourceManager)
{
    for (const ResourceTypeDescriptor* d : GetResourceTypeRegistry().All())
    {
        if (d && d->importer)
            d->importer->mResourceManager = resourceManager;
    }
}

bool ImporterManager::Import(ResourceType type, const MetaFileData& metaFileData)
{
    Importer* imp = ImporterFor(type);
    return imp ? imp->Import(metaFileData) : false;
}

bool ImporterManager::Save(ResourceType type, const MetaFileData& metaFileData, Resource*& inResource)
{
    Importer* imp = ImporterFor(type);
    return imp ? imp->Save(metaFileData, inResource) : false;
}

bool ImporterManager::Deserialize(ResourceType type, const std::string& libraryPath, Resource* resource)
{
    Importer* imp = ImporterFor(type);
    return imp ? imp->Deserialize(libraryPath, resource) : false;
}

void ImporterManager::Evict(ResourceType type, Resource* resource)
{
    if (Importer* imp = ImporterFor(type))
        imp->Evict(resource);
}

bool ImporterManager::Upload(ResourceType type, Resource* resource, IGPUResourceFactory* gpu)
{
    Importer* imp = ImporterFor(type);
    return imp ? imp->Upload(resource, gpu) : false;
}

void ImporterManager::Release(ResourceType type, Resource* resource, IGPUResourceFactory* gpu)
{
    if (Importer* imp = ImporterFor(type))
        imp->Release(resource, gpu);
}
