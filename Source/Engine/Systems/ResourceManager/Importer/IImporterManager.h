#ifndef IIMPORTERMANAGER_H
#define IIMPORTERMANAGER_H

#include <string>

class Resource;
enum class ResourceType : int8_t;
struct MetaFileData;
class ModuleResourceManager;

class IImporterManager
{
public:
    virtual ~IImporterManager() = default;

    virtual void Init(ModuleResourceManager* resourceManager) = 0;
    virtual bool Import(ResourceType type, const MetaFileData& metaFileData) = 0;
    virtual bool Deserialize(ResourceType type, const std::string& libraryPath, Resource* resource) = 0;
    virtual void Evict(ResourceType type, Resource* resource) = 0;
};

#endif // IIMPORTERMANAGER_H
