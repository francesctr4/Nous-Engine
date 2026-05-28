#pragma once

#include "Engine/Core/Globals.h"

#include <cstdint>
#include <string>

class Resource;
class ResourceMesh;
enum class ResourceType : int8_t;

// Minimal interface for loading and requesting resources.
// Implemented by ModuleResourceManager; used by ScenePreloader so that
// ScenePreloader can live in Systems/ without depending on Modules/.
class IResourceLoader
{
public:
    virtual ~IResourceLoader() = default;

    virtual Resource* CreateResource(const std::string& assetsPath) = 0;

    virtual Resource* CreateResourceFromLibrary(uint32 uid, ResourceType type,
                                                const std::string& name,
                                                const std::string& assetsPath,
                                                const std::string& libraryPath) = 0;

    virtual ResourceMesh* RequestOrCreateSubMeshResource(
        const std::string& assetsPath, int32_t submeshIndex) = 0;

    virtual ResourceMesh* RequestOrCreateSubMeshResourceFromLibrary(
        const std::string& libraryPath, int32_t submeshIndex,
        const std::string& assetsPath, uint32 hintUID = 0) = 0;
};
