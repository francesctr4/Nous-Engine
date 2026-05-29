#include "Engine/Systems/ResourceManager/Core/ImporterManager/include/ImporterManager.h"

#include "Engine/Systems/ResourceManager/Core/ImporterManager/include/IImporter.h"
#include "Engine/Systems/ResourceManager/Core/Resource/include/Resource.h"
#include "Engine/Systems/ResourceManager/Core/TypeRegistry/include/TypeRegistry.h"

namespace
{
    IAssetImporter* AssetImporterFor(ResourceType type)
    {
        const TypeDescriptor* d = GetTypeRegistry().Get(type);
        return d ? d->importer.get() : nullptr;
    }

    // Null for pipeline-only types (e.g. SCENE) — the lifecycle dispatchers below
    // treat that as a no-op rather than relying on stub implementations.
    IResourceImporter* ResourceImporterFor(ResourceType type)
    {
        const TypeDescriptor* d = GetTypeRegistry().Get(type);
        return d ? d->resourceImporter : nullptr;
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
    IAssetImporter* imp = AssetImporterFor(type);
    return imp ? imp->Import(metaFileData) : false;
}

bool ImporterManager::Save(ResourceType type, const MetaFileData& metaFileData, Resource*& inResource)
{
    IResourceImporter* imp = ResourceImporterFor(type);
    return imp ? imp->Save(metaFileData, inResource) : false;
}

bool ImporterManager::Deserialize(ResourceType type, const std::string& libraryPath, Resource* resource)
{
    IResourceImporter* imp = ResourceImporterFor(type);
    return imp ? imp->Deserialize(libraryPath, resource) : false;
}

void ImporterManager::Evict(ResourceType type, Resource* resource)
{
    if (IResourceImporter* imp = ResourceImporterFor(type))
        imp->Evict(resource);
}

bool ImporterManager::Upload(ResourceType type, Resource* resource, IGPUResourceFactory* gpu)
{
    IResourceImporter* imp = ResourceImporterFor(type);
    return imp ? imp->Upload(resource, gpu) : false;
}

void ImporterManager::Release(ResourceType type, Resource* resource, IGPUResourceFactory* gpu)
{
    if (IResourceImporter* imp = ResourceImporterFor(type))
        imp->Release(resource, gpu);
}
