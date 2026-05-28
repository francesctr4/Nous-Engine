#ifndef IMPORTERMANAGER_H
#define IMPORTERMANAGER_H

#include "IImporterManager.h"

class Resource;
enum class ResourceType : int8_t;

struct MetaFileData;
class IGPUResourceFactory;
class ModuleResourceManager;

class ImporterManager : public IImporterManager
{
public:

    // IImporterManager overrides
    void Init(ModuleResourceManager* resourceManager) override;
    bool Import(ResourceType type, const MetaFileData& metaFileData) override;
    bool Deserialize(ResourceType type, const std::string& libraryPath, Resource* resource) override;
    void Evict(ResourceType type, Resource* resource) override;
    bool Upload(ResourceType type, Resource* resource, IGPUResourceFactory* gpu) override;
    void Release(ResourceType type, Resource* resource, IGPUResourceFactory* gpu) override;

    // Asset pipeline (non-virtual)
    static bool Save(ResourceType type, const MetaFileData& metaFileData, Resource*& inResource);
};

#endif // IMPORTERMANAGER_H
