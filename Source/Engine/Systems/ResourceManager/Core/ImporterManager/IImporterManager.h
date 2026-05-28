#ifndef IIMPORTERMANAGER_H
#define IIMPORTERMANAGER_H

#include <string>

class Resource;
enum class ResourceType : int8_t;
struct MetaFileData;
class IGPUResourceFactory;
class ModuleResourceManager;

class IImporterManager
{
public:
    virtual ~IImporterManager() = default;

    virtual void Init(ModuleResourceManager* resourceManager) = 0;
    virtual bool Import(ResourceType type, const MetaFileData& metaFileData) = 0;
    virtual bool Deserialize(ResourceType type, const std::string& libraryPath, Resource* resource) = 0;
    virtual void Evict(ResourceType type, Resource* resource) = 0;

    // GPU only — must be called from the render thread.
    virtual bool Upload(ResourceType type, Resource* resource, IGPUResourceFactory* gpu) = 0;
    virtual void Release(ResourceType type, Resource* resource, IGPUResourceFactory* gpu) = 0;
};

#endif // IIMPORTERMANAGER_H
