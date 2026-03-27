#ifndef IMPORTERMANAGER_H
#define IMPORTERMANAGER_H

#include <Engine/Core/Globals.h>
#include <memory>

class Resource;
enum class ResourceType;

struct Importer;
struct MetaFileData;
class IGPUResourceFactory;
class ModuleResourceManager;

constexpr int16 c_NUM_IMPORTERS = 4;

class ImporterManager
{
public:

    static void Init(ModuleResourceManager* resourceManager);

    // Asset pipeline
    static bool Import(const ResourceType& type, const MetaFileData& metaFileData);
    static bool Save(const ResourceType& type, const MetaFileData& metaFileData, Resource*& inResource);

    // CPU only — safe to call from a job thread
    static bool Deserialize(const ResourceType& type, const std::string& libraryPath, Resource* resource);
    static void Evict(const ResourceType& type, Resource* resource);

    // GPU only — must be called from the render thread
    static bool Upload(const ResourceType& type, Resource* resource, IGPUResourceFactory* gpu);
    static void Release(const ResourceType& type, Resource* resource, IGPUResourceFactory* gpu);

private:

    static const std::array<std::unique_ptr<Importer>, c_NUM_IMPORTERS> importers;

};

#endif // IMPORTERMANAGER_H