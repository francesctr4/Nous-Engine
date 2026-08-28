#pragma once

#include <EngineCore/EngineExport.h>

#include <cstdint>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class ResourceBase;
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
    using CacheKey = std::pair<uint32_t, int32_t>;

    NOUS_ENGINE_API SubMeshCache(ResourceTable& table, ResourceQueue& uploads);

    // EDITOR path - resolves assetsPath via .meta file to get base UID + library path.
    NOUS_ENGINE_API ResourceMesh* RequestOrCreate(const std::string& assetsPath, int32_t submeshIndex);

    // GAME path - derives a synthetic base UID from hash(libraryPath).
    // Pass hintUID (e.g. from the scene file's "resourceUID" field) so that newly
    // created resources get a stable UID instead of a random one.
    NOUS_ENGINE_API ResourceMesh* RequestOrCreateFromLibrary(
        const std::string& libraryPath, int32_t submeshIndex,
        const std::string& assetsPath = "", uint32_t hintUID = 0);

    // Remove all index entries that map to the given UID.
    // Caller MUST hold the ResourceTable's lock.
    NOUS_ENGINE_API void EraseUID(uint32_t uid);

    // Drop all entries. Called from ClearResources during full teardown.
    NOUS_ENGINE_API void Clear();

private:
    ResourceMesh* BuildAndRegister(
        CacheKey key,
        const std::string& libraryPath,
        int32_t submeshIndex,
        const std::string& assetsPath,
        uint32_t hintUID = 0);

    // Removes the m_index entry at `mapIt` and the matching key from the
    // corresponding m_reverseIndex bucket. Caller MUST hold the
    // ResourceTable's lock (same contract as EraseUID).
    void EraseIndexEntry(std::map<CacheKey, uint32_t>::iterator mapIt);

    // Primary index: (baseUID, submeshIndex) -> resource UID.
    std::map<CacheKey, uint32_t>                             m_index;

    // Reverse index: resource UID -> the CacheKeys that point at it. Keeps
    // EraseUID O(k) instead of O(n) — k = number of submeshes sharing a UID,
    // typically 1, bounded by the submesh count of the source mesh.
    std::unordered_map<uint32_t, std::vector<CacheKey>>      m_reverseIndex;

    ResourceTable&             m_table;
    ResourceQueue&             m_uploads;
};
