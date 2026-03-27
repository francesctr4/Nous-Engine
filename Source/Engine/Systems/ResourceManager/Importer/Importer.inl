#ifndef IMPORTER_INL
#define IMPORTER_INL

#include <Engine/Core/Globals.h>
#include <string>

class Resource;
struct MetaFileData;
class RendererFrontend;
class ModuleResourceManager;

struct Importer
{
    virtual ~Importer() = default;

    virtual bool Import(const MetaFileData& metaFileData) = 0;
    virtual bool Save(const MetaFileData& metaFileData, Resource*& inResource) = 0;
    virtual bool Load(const std::string& libraryPath, Resource* outResource) = 0;
    virtual bool Unload(Resource* inResource) = 0;

    RendererFrontend*      mRendererFrontend = nullptr;
    ModuleResourceManager* mResourceManager  = nullptr;
};

#endif // IMPORTER_INL