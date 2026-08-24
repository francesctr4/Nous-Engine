#pragma once

#include "Engine/Core/Globals.h"

#include <cstdint>
#include <string>

class ResourceBase;
class ResourceMaterial;
class ResourceMesh;
enum class ResourceType : int8_t;

// The resource system, as seen from inside Systems/.
// Implemented by ModuleResourceManager so that consumers in Systems/ (ScenePreloader,
// the ECS components, the importers) can load, release and import resources without
// depending on Modules/.
//
// Mostly load/request operations; ImportFile is the one asset-pipeline entry point,
// added because ImporterMesh must import the peer textures/materials it discovers
// inside a model file, and no load-oriented call can express that.
class IResourceLoader
{
public:
    virtual ~IResourceLoader() = default;

    virtual ResourceBase* CreateResource(const std::string& assetsPath) = 0;

    virtual ResourceBase* CreateResourceFromLibrary(uint32 uid, ResourceType type,
                                                const std::string& name,
                                                const std::string& assetsPath,
                                                const std::string& libraryPath) = 0;

    virtual ResourceMesh* RequestOrCreateSubMeshResource(
        const std::string& assetsPath, int32_t submeshIndex) = 0;

    virtual ResourceMesh* RequestOrCreateSubMeshResourceFromLibrary(
        const std::string& libraryPath, int32_t submeshIndex,
        const std::string& assetsPath, uint32 hintUID = 0) = 0;

    // Drops one reference to a loaded resource; frees it when the count hits zero.
    // Returns false if the UID is not currently loaded.
    virtual bool UnloadResource(uint32 uid) = 0;

    // The engine's fallback material. Borrowed — do NOT UnloadResource() it.
    virtual ResourceMaterial* GetDefaultMaterial() const = 0;

    // Runs the asset-import pipeline on one file (writes its .meta, mirrors it into
    // Library/). Used by ImporterMesh to import the peer textures and materials it
    // discovers inside a model. Returns false if the import failed.
    virtual bool ImportFile(const std::string& path) = 0;
};
