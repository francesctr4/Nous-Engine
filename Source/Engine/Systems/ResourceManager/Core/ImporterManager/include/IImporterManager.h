#pragma once

#include "IImporterDispatcher.h"

#include <string>

class ResourceBase;
enum class ResourceType : int8_t;
struct MetaFileData;
class IGPUResourceFactory;
class ModuleResourceManager;

// Full importer-manager surface: the import dispatch (inherited from
// IImporterDispatcher) plus the runtime resource lifecycle. The owner
// (ModuleResourceManager) and the render thread (ModuleRenderer3D) need the
// lifecycle half; the asset pipeline (ImportPipeline / HotReloader) only needs
// the inherited Import and so depends on IImporterDispatcher directly.
class IImporterManager : public IImporterDispatcher
{
public:
    virtual void Init(ModuleResourceManager* resourceManager) = 0;
    virtual bool Deserialize(ResourceType type, const std::string& libraryPath, ResourceBase* resource) = 0;
    virtual void Evict(ResourceType type, ResourceBase* resource) = 0;

    // GPU only — must be called from the render thread.
    virtual bool Upload(ResourceType type, ResourceBase* resource, IGPUResourceFactory* gpu) = 0;
    virtual void Release(ResourceType type, ResourceBase* resource, IGPUResourceFactory* gpu) = 0;
};
