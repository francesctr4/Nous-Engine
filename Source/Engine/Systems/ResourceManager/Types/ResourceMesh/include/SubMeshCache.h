#pragma once

#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"

#include <cstdint>
#include <map>
#include <string>
#include <utility>

class Resource;
class ResourceMesh;
class ResourceTable;
class ResourceQueue;
enum class ResourceType : int8_t;

// Owns the (baseUID, submeshIndex) -> uid secondary index and handles all
// sub-resource deduplication. Extracted from ModuleResourceManager.
//
// Locking contract:
//   - RequestOrCreate*, BuildAndRegister: acquire the ResourceTable's lock
//     internally. Caller must NOT hold it on entry.
//   - EraseUID: caller MUST hold the ResourceTable's lock. ModuleResourceManager
//     holds it across the call from DeleteResource.
//   - Clear: no locking required (called during full teardown).
class SubMeshCache
{
public:
    using CacheKey = std::pair<uint32, int32_t>;

    NOUS_ENGINE_API SubMeshCache(ResourceTable& table, ResourceQueue& uploads);

    // EDITOR path - resolves assetsPath via .meta file to get base UID + library path.
    NOUS_ENGINE_API ResourceMesh* RequestOrCreate(const std::string& assetsPath, int32_t submeshIndex);

    // GAME path - derives a synthetic base UID from hash(libraryPath).
    // Pass hintUID (e.g. from the scene file's "resourceUID" field) so that newly
    // created resources get a stable UID instead of a random one.
    NOUS_ENGINE_API ResourceMesh* RequestOrCreateFromLibrary(
        const std::string& libraryPath, int32_t submeshIndex,
        const std::string& assetsPath = "", uint32 hintUID = 0);

    // Remove all index entries that map to the given UID.
    // Caller MUST hold the ResourceTable's lock.
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

    ResourceTable&             m_table;
    ResourceQueue&             m_uploads;
};
