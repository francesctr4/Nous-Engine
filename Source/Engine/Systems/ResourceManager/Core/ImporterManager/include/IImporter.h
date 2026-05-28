#pragma once

#include <string>

class Resource;
struct MetaFileData;
class IGPUResourceFactory;
class ModuleResourceManager;

struct IImporter
{
    virtual ~IImporter() = default;

    // Asset pipeline — file I/O only, no GPU involvement.
    virtual bool Import(const MetaFileData& metaFileData) = 0;
    virtual bool Save(const MetaFileData& metaFileData, Resource*& inResource) = 0;

    // CPU only — safe to call from a job thread.
    // Deserialize reads the library binary and populates CPU-side resource fields.
    // Evict releases CPU data (vectors, buffers) after the GPU copy is done.
    virtual bool Deserialize(const std::string& libraryPath, Resource* resource) = 0;
    virtual void Evict(Resource* resource) = 0;

    // GPU only — must be called from the render thread.
    // Upload transfers CPU data to the GPU and sets GPU handles on the resource.
    // Release frees GPU handles; call Evict afterward to drop the CPU copy.
    virtual bool Upload(Resource* resource, IGPUResourceFactory* gpu) = 0;
    virtual void Release(Resource* resource, IGPUResourceFactory* gpu) = 0;

    ModuleResourceManager* m_resourceManager = nullptr;
};