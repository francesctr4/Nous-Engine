#pragma once

#include <string>

class ResourceBase;
struct MetaFileData;
class IGPUResourceFactory;
class IResourceLoader;

// Asset-pipeline importer — file I/O only, no runtime resource object and no GPU
// residency. This is all a "pipeline-only" type (e.g. SCENE) needs to implement:
// the import step mirrors the source asset into its Library/ destination.
struct IAssetImporter
{
    virtual ~IAssetImporter() = default;

    // Asset pipeline — file I/O only, no GPU involvement.
    virtual bool Import(const MetaFileData& metaFileData) = 0;

    // The resource registry, as an interface — keeps Systems/ off Modules/.
    // Assigned once by ImporterManager::Init. May be null in tests.
    IResourceLoader* m_resources = nullptr;
};

// Resource importer — a type that owns a runtime `Resource` object with a CPU/GPU
// lifecycle. Adds Save plus the load/evict/upload/release contract on top of the
// asset pipeline. Implement this (not IAssetImporter) for any type that has a
// createFn/destroyFn in its TypeDescriptor.
struct IResourceImporter : IAssetImporter
{
    // Asset pipeline — writes a runtime resource back to its source/library form.
    virtual bool Save(const MetaFileData& metaFileData, ResourceBase*& inResource) = 0;

    // CPU only — safe to call from a job thread.
    // Deserialize reads the library binary and populates CPU-side resource fields.
    // Evict releases CPU data (vectors, buffers) after the GPU copy is done.
    virtual bool Deserialize(const std::string& libraryPath, ResourceBase* resource) = 0;
    virtual void Evict(ResourceBase* resource) = 0;

    // GPU only — must be called from the render thread.
    // Upload transfers CPU data to the GPU and sets GPU handles on the resource.
    // Release frees GPU handles; call Evict afterward to drop the CPU copy.
    virtual bool Upload(ResourceBase* resource, IGPUResourceFactory* gpu) = 0;
    virtual void Release(ResourceBase* resource, IGPUResourceFactory* gpu) = 0;
};
