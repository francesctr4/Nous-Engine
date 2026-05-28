#include "Engine/Systems/ResourceManager/Types/ResourceMesh/include/SubMeshCache.h"

#include "Engine/Systems/ResourceManager/Types/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/Runtime/ImportPipeline/include/ImportPipeline.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMesh/include/ImporterMesh.h"
#include "Engine/Systems/ResourceManager/Core/Resource/include/MetaFileData.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Utils/Serialization/Random/Random.h"
#include "Engine/Core/Logger/Logger.h"

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_RESOURCEMANAGER;

SubMeshCache::SubMeshCache(
    std::unordered_map<uint32, Resource*>& resources,
    std::mutex& resourcesMutex,
    std::vector<std::pair<ResourceType, Resource*>>& pendingUploads,
    std::mutex& pendingUploadsMutex)
    : m_resources(resources)
    , m_resourcesMutex(resourcesMutex)
    , m_pendingUploads(pendingUploads)
    , m_pendingUploadsMutex(pendingUploadsMutex)
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
        std::lock_guard lock(m_resourcesMutex);
        if (const auto mapIt = m_index.find(key); mapIt != m_index.end())
        {
            if (const auto resIt = m_resources.find(mapIt->second);
                resIt != m_resources.end() && resIt->second)
            {
                resIt->second->IncreaseReferenceCount();
                return down_cast<ResourceMesh*>(resIt->second);
            }
            // Stale entry — resource was destroyed; remove and recreate below.
            m_index.erase(mapIt);
        }
    }

    return BuildAndRegister(key, metaData.libraryPath, submeshIndex, assetsPath);
}

ResourceMesh* SubMeshCache::RequestOrCreateFromLibrary(
    const std::string& libraryPath, int32_t submeshIndex,
    const std::string& assetsPath, uint32 hintUID)
{
    // Derive a stable synthetic base UID from the library path so dedup works
    // without a .meta file (GAME mode path).
    const auto baseUID = static_cast<uint32>(std::hash<std::string>{}(libraryPath) & 0xFFFFFFFF);
    const CacheKey key  = { baseUID, submeshIndex };

    {
        std::lock_guard lock(m_resourcesMutex);
        if (const auto mapIt = m_index.find(key); mapIt != m_index.end())
        {
            if (const auto resIt = m_resources.find(mapIt->second);
                resIt != m_resources.end() && resIt->second)
            {
                // Back-fill assetsPath if an earlier GAME-mode caller didn't have it
                // but this EDITOR-mode caller does (needed for scene serialization).
                if (!assetsPath.empty() && resIt->second->GetAssetsPath().empty())
                    resIt->second->SetAssetsPath(assetsPath);
                resIt->second->IncreaseReferenceCount();
                return down_cast<ResourceMesh*>(resIt->second);
            }
            m_index.erase(mapIt);
        }
    }

    return BuildAndRegister(key, libraryPath, submeshIndex, assetsPath, hintUID);
}

void SubMeshCache::EraseUID(uint32 uid)
{
    // Caller MUST hold m_resourcesMutex — see locking contract in SubMeshCache.h.
    for (auto it = m_index.begin(); it != m_index.end(); )
    {
        if (it->second == uid)
            it = m_index.erase(it);
        else
            ++it;
    }
}

void SubMeshCache::Clear()
{
    m_index.clear();
}

ResourceMesh* SubMeshCache::BuildAndRegister(
    CacheKey key,
    const std::string& libraryPath,
    int32_t submeshIndex,
    const std::string& assetsPath,
    uint32 hintUID)
{
    const auto hierarchy = ImporterMesh::LoadHierarchy(libraryPath);
    if (submeshIndex < 0 || submeshIndex >= static_cast<int32_t>(hierarchy.size()))
    {
        NOUS_ERROR("SubMeshCache::BuildAndRegister: index %d out of range (count=%zu) for '%s'",
            submeshIndex, hierarchy.size(), libraryPath.c_str());
        return nullptr;
    }

    const SubMeshData& sub = hierarchy[static_cast<size_t>(submeshIndex)];

    auto* mesh = NOUS_NEW<ResourceMesh>(MemoryTag::RESOURCE_MESH);
    mesh->SetName(sub.name);
    mesh->SetType(ResourceType::MESH);
    mesh->SetLibraryPath(libraryPath);
    if (!assetsPath.empty())
        mesh->SetAssetsPath(assetsPath);

    mesh->vertices = sub.vertices;
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

    uint32 uid;
    {
        std::lock_guard lock(m_resourcesMutex);
        // Prefer the caller's hint UID (e.g. from a scene file's "resourceUID" field)
        // so the UID stays stable across sessions. Fall back to random only if the
        // hint is 0 or already occupied by a different resource.
        if (hintUID != 0 && !m_resources.contains(hintUID))
            uid = hintUID;
        else
        {
            do { uid = static_cast<uint32>(Random::Generate()); }
            while (uid == 0 || m_resources.contains(uid));
        }
        m_resources[uid]  = mesh;
        m_index[key]      = uid;
    }
    {
        std::lock_guard lock(m_pendingUploadsMutex);
        m_pendingUploads.emplace_back(ResourceType::MESH, mesh);
    }

    mesh->SetUID(uid);
    mesh->IncreaseReferenceCount();
    mesh->Validate();
    return mesh;
}
