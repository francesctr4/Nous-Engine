#include "Engine/Systems/ResourceManager/Core/ImporterManager/include/ImporterManager.h"

#include "Engine/Systems/ResourceManager/Core/ImporterManager/include/IImporter.h"
#include "Engine/Systems/ResourceManager/Core/Resource/include/Resource.h"
#include "Engine/Systems/ResourceManager/Core/TypeRegistry/include/TypeRegistry.h"

namespace
{
    IImporter* ImporterFor(ResourceType type)
    {
        const TypeDescriptor* d = GetTypeRegistry().Get(type);
        return d ? d->importer.get() : nullptr;
    }
}

void ImporterManager::Init(ModuleResourceManager* resourceManager)
{
    for (const TypeDescriptor* d : GetTypeRegistry().All())
    {
        if (d && d->importer)
            d->importer->m_resourceManager = resourceManager;
    }
}

bool ImporterManager::Import(ResourceType type, const MetaFileData& metaFileData)
{
    IImporter* imp = ImporterFor(type);
    return imp ? imp->Import(metaFileData) : false;
}

bool ImporterManager::Save(ResourceType type, const MetaFileData& metaFileData, Resource*& inResource)
{
    IImporter* imp = ImporterFor(type);
    return imp ? imp->Save(metaFileData, inResource) : false;
}

bool ImporterManager::Deserialize(ResourceType type, const std::string& libraryPath, Resource* resource)
{
    IImporter* imp = ImporterFor(type);
    return imp ? imp->Deserialize(libraryPath, resource) : false;
}

void ImporterManager::Evict(ResourceType type, Resource* resource)
{
    if (IImporter* imp = ImporterFor(type))
        imp->Evict(resource);
}

bool ImporterManager::Upload(ResourceType type, Resource* resource, IGPUResourceFactory* gpu)
{
    IImporter* imp = ImporterFor(type);
    return imp ? imp->Upload(resource, gpu) : false;
}

void ImporterManager::Release(ResourceType type, Resource* resource, IGPUResourceFactory* gpu)
{
    if (IImporter* imp = ImporterFor(type))
        imp->Release(resource, gpu);
}
