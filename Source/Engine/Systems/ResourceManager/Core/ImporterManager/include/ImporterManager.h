#pragma once

#include "IImporterManager.h"
#include "Engine/EngineExport.h"

class Resource;
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
    void Init(ModuleResourceManager* resourceManager) override;
    bool Import(ResourceType type, const MetaFileData& metaFileData) override;
    bool Deserialize(ResourceType type, const std::string& libraryPath, Resource* resource) override;
    void Evict(ResourceType type, Resource* resource) override;
    bool Upload(ResourceType type, Resource* resource, IGPUResourceFactory* gpu) override;
    void Release(ResourceType type, Resource* resource, IGPUResourceFactory* gpu) override;

private:
    const TypeRegistry* m_typeRegistry = nullptr;
};
