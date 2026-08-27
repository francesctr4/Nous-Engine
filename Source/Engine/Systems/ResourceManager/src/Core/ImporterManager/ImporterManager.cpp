#include <ResourceManager/Core/ImporterManager.h>

#include <ResourceManager/Core/IImporter.h>
#include <ResourceManager/Core/ResourceBase.h>
#include <ResourceManager/Core/TypeRegistry.h>

namespace
{
    IAssetImporter* AssetImporterFor(const TypeRegistry& reg, ResourceType type)
    {
        const TypeDescriptor* d = reg.Get(type);
        return d ? d->importer.get() : nullptr;
    }

    // Null for pipeline-only types (e.g. SCENE) — the lifecycle dispatchers below
    // treat that as a no-op rather than relying on stub implementations.
    IResourceImporter* ResourceImporterFor(const TypeRegistry& reg, ResourceType type)
    {
        const TypeDescriptor* d = reg.Get(type);
        return d ? d->resourceImporter : nullptr;
    }
}

ImporterManager::ImporterManager(const TypeRegistry& typeRegistry)
    : m_typeRegistry(&typeRegistry)
{
}

void ImporterManager::Init(IResourceLoader* resources)
{
    for (const TypeDescriptor* d : m_typeRegistry->All())
    {
        if (d && d->importer)
            d->importer->m_resources = resources;
    }
}

bool ImporterManager::Import(ResourceType type, const MetaFileData& metaFileData)
{
    IAssetImporter* imp = AssetImporterFor(*m_typeRegistry, type);
    return imp ? imp->Import(metaFileData) : false;
}

bool ImporterManager::Deserialize(ResourceType type, const std::string& libraryPath, ResourceBase* resource)
{
    IResourceImporter* imp = ResourceImporterFor(*m_typeRegistry, type);
    return imp ? imp->Deserialize(libraryPath, resource) : false;
}

void ImporterManager::Evict(ResourceType type, ResourceBase* resource)
{
    if (IResourceImporter* imp = ResourceImporterFor(*m_typeRegistry, type))
        imp->Evict(resource);
}

bool ImporterManager::Upload(ResourceType type, ResourceBase* resource, IGPUResourceFactory* gpu)
{
    IResourceImporter* imp = ResourceImporterFor(*m_typeRegistry, type);
    return imp ? imp->Upload(resource, gpu) : false;
}

void ImporterManager::Release(ResourceType type, ResourceBase* resource, IGPUResourceFactory* gpu)
{
    if (IResourceImporter* imp = ResourceImporterFor(*m_typeRegistry, type))
        imp->Release(resource, gpu);
}
