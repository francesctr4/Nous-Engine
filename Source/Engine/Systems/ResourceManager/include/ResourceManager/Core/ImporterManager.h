#pragma once

#include "IImporterManager.h"
#include "Engine/EngineExport.h"

class ResourceBase;
enum class ResourceType : int8_t;

struct MetaFileData;
class IGPUResourceFactory;
class ModuleResourceManager;
class TypeRegistry;

class ImporterManager : public IImporterManager
{
public:

    NOUS_ENGINE_API explicit ImporterManager(const TypeRegistry& typeRegistry);

    // IImporterManager overrides
    void Init(IResourceLoader* resources) override;
    bool Import(ResourceType type, const MetaFileData& metaFileData) override;
    bool Deserialize(ResourceType type, const std::string& libraryPath, ResourceBase* resource) override;
    void Evict(ResourceType type, ResourceBase* resource) override;
    bool Upload(ResourceType type, ResourceBase* resource, IGPUResourceFactory* gpu) override;
    void Release(ResourceType type, ResourceBase* resource, IGPUResourceFactory* gpu) override;

private:
    const TypeRegistry* m_typeRegistry = nullptr;
};
