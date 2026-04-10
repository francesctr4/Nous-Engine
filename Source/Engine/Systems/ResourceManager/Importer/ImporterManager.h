#ifndef IMPORTERMANAGER_H
#define IMPORTERMANAGER_H

#include <Engine/Core/Globals.h>
#include <memory>

#include "Engine/Systems/ResourceManager/Importer/IImporterManager.h"

class Resource;
enum class ResourceType : int8_t;

struct Importer;
struct MetaFileData;
class IGPUResourceFactory;
class ModuleResourceManager;

constexpr int16 c_NUM_IMPORTERS = 4;

class ImporterManager : public IImporterManager
{
public:

    // IImporterManager — CPU-side, virtual, injectable
    void Init(ModuleResourceManager* resourceManager) override;
    bool Import(const ResourceType& type, const MetaFileData& metaFileData) override;
    bool Deserialize(const ResourceType& type, const std::string& libraryPath, Resource* resource) override;
    void Evict(const ResourceType& type, Resource* resource) override;

    // Asset pipeline (non-virtual)
    static bool Save(const ResourceType& type, const MetaFileData& metaFileData, Resource*& inResource);

    // GPU only — must be called from the render thread (used directly by ModuleRenderer3D)
    static bool Upload(const ResourceType& type, Resource* resource, IGPUResourceFactory* gpu);
    static void Release(const ResourceType& type, Resource* resource, IGPUResourceFactory* gpu);

private:

    static const std::array<std::unique_ptr<Importer>, c_NUM_IMPORTERS> importers;

};

#endif // IMPORTERMANAGER_H