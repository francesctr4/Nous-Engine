#pragma once

#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class Resource;
class ResourceMesh;
enum class ResourceType : int8_t;

// Owns the (baseUID, submeshIndex) → uid secondary index and handles all
// sub-resource deduplication. Extracted from ModuleResourceManager.
//
// Holds non-owning references to ModuleResourceManager's private registry
// data — valid for the lifetime of the owning ModuleResourceManager.
//
// Locking contract:
//   - RequestOrCreate*, BuildAndRegister: acquire m_resourcesMutex internally.
//     Caller must NOT hold it on entry.
//   - EraseUID: caller MUST hold m_resourcesMutex (DeleteResource holds it
//     for the full erase pass and calls EraseUID inside that scope).
//   - Clear: no locking required (called during full teardown).
class SubMeshCache
{
public:
    using CacheKey = std::pair<uint32, int32_t>;

    NOUS_ENGINE_API SubMeshCache(
        std::unordered_map<uint32, Resource*>& resources,
        std::mutex& resourcesMutex,
        std::vector<std::pair<ResourceType, Resource*>>& pendingUploads,
        std::mutex& pendingUploadsMutex);

    // EDITOR path — resolves assetsPath via .meta file to get base UID + library path.
    NOUS_ENGINE_API ResourceMesh* RequestOrCreate(const std::string& assetsPath, int32_t submeshIndex);

    // GAME path — derives a synthetic base UID from hash(libraryPath).
    // Pass hintUID (e.g. from the scene file's "resourceUID" field) so that newly
    // created resources get a stable UID instead of a random one.
    NOUS_ENGINE_API ResourceMesh* RequestOrCreateFromLibrary(
        const std::string& libraryPath, int32_t submeshIndex,
        const std::string& assetsPath = "", uint32 hintUID = 0);

    // Remove all index entries that map to the given UID.
    // Caller MUST hold m_resourcesMutex.
    NOUS_ENGINE_API void EraseUID(uint32 uid);

    // Drop all entries. Called from ClearResources during full teardown.
    NOUS_ENGINE_API void Clear();

private:
    ResourceMesh* BuildAndRegister(
        CacheKey key,
        const std::string& libraryPath,
        int32_t submeshIndex,
        const std::string& assetsPath,
        uint32 hintUID = 0);

    std::map<CacheKey, uint32> m_index;

    std::unordered_map<uint32, Resource*>&           m_resources;
    std::mutex&                                       m_resourcesMutex;
    std::vector<std::pair<ResourceType, Resource*>>& m_pendingUploads;
    std::mutex&                                       m_pendingUploadsMutex;
};
