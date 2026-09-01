#include <ResourceManager/Types/ResourceMesh/SubMeshCache.h>
#include <EngineCore/Casts.h>

#include <Logger/Logger.h>
#include <MemoryManager/MemoryManager.h>
#include <ResourceManager/Core/MetaFileData.h>
#include <ResourceManager/Core/ResourceQueue.h>
#include <ResourceManager/Core/ResourceTable.h>
#include <ResourceManager/Runtime/ImportPipeline.h>
#include <ResourceManager/Types/ResourceMesh/ImporterMesh.h>
#include <ResourceManager/Types/ResourceMesh/ResourceMesh.h>
#include <Utils/Serialization/Random.h>
#include <cstddef>

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_RESOURCEMANAGER;

SubMeshCache::SubMeshCache(ResourceTable& table, ResourceQueue& uploads)
    : m_table(table)
    , m_uploads(uploads)
{
}

ResourceMesh* SubMeshCache::RequestOrCreate(const std::string& assetsPath, int32_t submeshIndex)
{
    MetaFileData metaData;
    if (!ImportPipeline::GetAssetMetaData(assetsPath, metaData))
    {
        NOUS_ERROR("SubMeshCache::RequestOrCreate: missing meta for '%s'", assetsPath.c_str());
        return nullptr;
    }

    const CacheKey key = { metaData.uid, submeshIndex };

    {
        ResourceTable::ScopedLock lock(m_table);
        auto& resources = lock.Map();
        if (const auto mapIt = m_index.find(key); mapIt != m_index.end())
        {
            if (const auto resIt = resources.find(mapIt->second);
                resIt != resources.end() && resIt->second)
            {
                resIt->second->IncreaseReferenceCount();
                return down_cast<ResourceMesh*>(resIt->second);
            }
            // Stale entry — resource was destroyed; remove and recreate below.
            EraseIndexEntry(mapIt);
        }
    }

    return BuildAndRegister(key, metaData.libraryPath, submeshIndex, assetsPath);
}

ResourceMesh* SubMeshCache::RequestOrCreateFromLibrary(
    const std::string& libraryPath, int32_t submeshIndex,
    const std::string& assetsPath, uint32_t hintUID)
{
    // Derive a stable synthetic base UID from the library path so dedup works
    // without a .meta file (GAME mode path).
    const auto baseUID = static_cast<uint32_t>(std::hash<std::string>{}(libraryPath) & 0xFFFFFFFF);
    const CacheKey key  = { baseUID, submeshIndex };

    {
        ResourceTable::ScopedLock lock(m_table);
        auto& resources = lock.Map();
        if (const auto mapIt = m_index.find(key); mapIt != m_index.end())
        {
            if (const auto resIt = resources.find(mapIt->second);
                resIt != resources.end() && resIt->second)
            {
                // Back-fill assetsPath if an earlier GAME-mode caller didn't have it
                // but this EDITOR-mode caller does (needed for scene serialization).
                if (!assetsPath.empty() && resIt->second->GetAssetsPath().empty())
                    resIt->second->SetAssetsPath(assetsPath);
                resIt->second->IncreaseReferenceCount();
                return down_cast<ResourceMesh*>(resIt->second);
            }
            EraseIndexEntry(mapIt);
        }
    }

    return BuildAndRegister(key, libraryPath, submeshIndex, assetsPath, hintUID);
}

void SubMeshCache::EraseUID(uint32_t uid)
{
    // Caller MUST hold the ResourceTable's lock — see locking contract in SubMeshCache.h.
    const auto rev = m_reverseIndex.find(uid);
    if (rev == m_reverseIndex.end()) return;
    for (const CacheKey& key : rev->second)
        m_index.erase(key);
    m_reverseIndex.erase(rev);
}

void SubMeshCache::Clear()
{
    m_index.clear();
    m_reverseIndex.clear();
}

void SubMeshCache::EraseIndexEntry(std::map<CacheKey, uint32_t>::iterator mapIt)
{
    const uint32_t   uid = mapIt->second;
    const CacheKey key = mapIt->first;
    m_index.erase(mapIt);

    const auto rev = m_reverseIndex.find(uid);
    if (rev == m_reverseIndex.end()) return;
    std::erase(rev->second, key);
    if (rev->second.empty()) m_reverseIndex.erase(rev);
}

ResourceMesh* SubMeshCache::BuildAndRegister(
    CacheKey key,
    const std::string& libraryPath,
    int32_t submeshIndex,
    const std::string& assetsPath,
    uint32_t hintUID)
{
    // Reads ONLY this submesh. It used to call LoadHierarchy, which deserialized
    // every submesh in the file and then kept one -- and since SpawnMeshAsHierarchy
    // fans this call out across worker threads, one N-submesh model was parsed N+1
    // times, N of them concurrently: O(N^2) bytes for an O(N) result. The V4 offset
    // directory is what makes a single-submesh read possible.
    SubMeshData sub;
    if (!ImporterMesh::LoadSubmesh(libraryPath, submeshIndex, sub))
    {
        NOUS_ERROR("SubMeshCache::BuildAndRegister: failed to load submesh %d of '%s'",
            submeshIndex, libraryPath.c_str());
        return nullptr;
    }

    auto* mesh = NOUS_NEW<ResourceMesh>(MemoryTag::RESOURCE_MESH);
    mesh->SetName(sub.name);
    mesh->SetType(ResourceType::MESH);
    mesh->SetLibraryPath(libraryPath);
    if (!assetsPath.empty())
        mesh->SetAssetsPath(assetsPath);

    // `sub` is a local now, not a reference into a shared vector, so the geometry
    // moves instead of copying.
    mesh->vertices = std::move(sub.vertices);
    mesh->indices.assign(sub.indices.begin(), sub.indices.end());

    if (!mesh->vertices.empty())
    {
        mesh->localAABBMin = mesh->vertices[0].position;
        mesh->localAABBMax = mesh->vertices[0].position;
        for (const auto& v : mesh->vertices)
        {
            mesh->localAABBMin = glm::min(mesh->localAABBMin, v.position);
            mesh->localAABBMax = glm::max(mesh->localAABBMax, v.position);
        }
    }

    mesh->SetState(ResourceState::CPU_READY);

    uint32_t uid;
    {
        ResourceTable::ScopedLock lock(m_table);
        auto& resources = lock.Map();
        // Prefer the caller's hint UID (e.g. from a scene file's "resourceUID" field)
        // so the UID stays stable across sessions. Fall back to random only if the
        // hint is 0 or already occupied by a different resource.
        if (hintUID != 0 && !resources.contains(hintUID))
            uid = hintUID;
        else
        {
            do { uid = static_cast<uint32_t>(Random::Generate()); }
            while (uid == 0 || resources.contains(uid));
        }
        resources[uid] = mesh;
        m_index[key]   = uid;
        m_reverseIndex[uid].push_back(key);
    }
    m_uploads.Push(ResourceType::MESH, mesh);

    mesh->SetUID(uid);
    mesh->IncreaseReferenceCount();
    return mesh;
}
