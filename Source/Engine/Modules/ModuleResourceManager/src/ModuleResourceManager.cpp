#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Renderer/IGPUResourceFactory.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include <filesystem>
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Systems/ResourceManager/Resource/MetaFileData.inl"

#include "Engine/Systems/ResourceManager/Importer/IImporterManager.h"
#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"

#include <future>
#include <ranges>
#include <utility>

#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"

#include <unordered_map>

// ---------------------------------------------------------------------------
// Resource factory table — one entry per ResourceType.
// Adding a new type: add one entry here; no other edits required.
// ---------------------------------------------------------------------------
namespace
{
    struct ResourceFactory
    {
        Resource* (*create)();
        void      (*destroy)(Resource*);
    };

    const std::unordered_map<ResourceType, ResourceFactory> k_ResourceFactories =
    {
        { ResourceType::MESH,
          { []() -> Resource* { return NOUS_NEW<ResourceMesh>(MemoryTag::RESOURCE_MESH); },
            [](Resource* r)   { NOUS_DELETE(r, MemoryTag::RESOURCE_MESH); } }},
        { ResourceType::MATERIAL,
          { []() -> Resource* { return NOUS_NEW<ResourceMaterial>(MemoryTag::RESOURCE_MATERIAL); },
            [](Resource* r)   { NOUS_DELETE(r, MemoryTag::RESOURCE_MATERIAL); } }},
        { ResourceType::TEXTURE,
          { []() -> Resource* { return NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE); },
            [](Resource* r)   { NOUS_DELETE(r, MemoryTag::RESOURCE_TEXTURE); } }},
        { ResourceType::SHADER,
          { []() -> Resource* { return NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER); },
            [](Resource* r)   { NOUS_DELETE(r, MemoryTag::RESOURCE_SHADER); } }},
    };

    Resource* InstantiateResource(const ResourceType type)
    {
        const auto it = k_ResourceFactories.find(type);
        if (it == k_ResourceFactories.end()) return nullptr;
        return it->second.create();
    }
}

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_RESOURCEMANAGER;

ModuleResourceManager::ModuleResourceManager(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem,
                                             IImporterManager* importerManager)
    : Module(eventSystem, jobSystem)
    , mImporterManager(importerManager)
    , m_importPipeline(importerManager, jobSystem)
    , m_scenePreloader(this)
    , m_subMeshCache(resources, resourcesMutex, m_pendingUploads, m_pendingUploadsMutex)
{
	eventSystem->Subscribe(EventType::DROP_FILE, this);
}

ModuleResourceManager::~ModuleResourceManager() = default;

bool ModuleResourceManager::Awake()
{
	mImporterManager->Init(this);

	// Always ensure directories exist — idempotent, safe to call in any mode.
	m_importPipeline.EnsureLibraryDirectories();

	return true;
}

void ModuleResourceManager::ScanAndImportAssets()
{
	m_importPipeline.ScanAndImportAssets();
}

bool ModuleResourceManager::ImportDirectory(const std::string& directory)
{
	return m_importPipeline.ImportDirectory(directory);
}

bool ModuleResourceManager::Start()
{
	NOUS_INFO("Queuing built-in textures and default material for GPU upload...");
	{
		auto uploads = m_builtinResources.Create();
		std::scoped_lock lock(m_pendingUploadsMutex);
		m_pendingUploads.insert(m_pendingUploads.end(), uploads.begin(), uploads.end());
	}
	return true;
}


UpdateStatus ModuleResourceManager::PreUpdate(float dt)
{
	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleResourceManager::Update(float dt)
{
	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleResourceManager::PostUpdate(float dt)
{
	return UpdateStatus::CONTINUE;
}

bool ModuleResourceManager::CleanUp()
{
	return true;
}

void ModuleResourceManager::OnEvent(const Event& event)
{
	if (event.type == EventType::DROP_FILE)
		ImportFile(event.ctx.c);
}

bool ModuleResourceManager::ImportFile(const std::string& path)
{
	return m_importPipeline.ImportFile(path);
}

std::unordered_map<uint32, Resource*> ModuleResourceManager::GetResourcesMap() const
{
	std::scoped_lock lock(resourcesMutex);
	return resources;
}


void ModuleResourceManager::DeleteResource(Resource*& resource)
{
	// Remove any sub-resource index entry that maps to this UID.
	// SubMeshCache::EraseUID requires resourcesMutex to be held by the caller.
	{
		const uint32 uid = resource->GetUID();
		std::scoped_lock lock(resourcesMutex);
		m_subMeshCache.EraseUID(uid);
	}

	if (const auto it = k_ResourceFactories.find(resource->GetType());
		it != k_ResourceFactories.end())
		it->second.destroy(resource);

	// NOTE: resources.erase(uid) is intentionally NOT here.
	// EvictResource removes the entry under resourcesMutex before calling DeleteResource,
	// so the map is clean before we reach this point.
}

bool ModuleResourceManager::ResourceExists(const uint32 uid) const
{
	std::scoped_lock lock(resourcesMutex);
	return resources.contains(uid);
}

bool ModuleResourceManager::ClaimSlot(const uint32 uid)
{
	std::scoped_lock lock(resourcesMutex);
	if (resources.contains(uid)) return false;
	resources[uid] = nullptr; // placeholder: marks slot as "in progress"
	return true;
}

Resource* ModuleResourceManager::SpinWaitForSlot(const uint32 uid)
{
	Resource* resource = nullptr;
	while (true)
	{
		{
			std::scoped_lock lock(resourcesMutex);
			const auto it = resources.find(uid);
			if (it == resources.end())
				return nullptr; // evicted before it resolved; caller should retry
			resource = it->second;
			if (resource != nullptr)
			{
				resource->IncreaseReferenceCount(); // under lock — closes the eviction race
				break;
			}
		}
		nous::engine::multithreading::NOUS_Thread::SleepMS(1);
	}
	resource->Validate();
	return resource;
}

Resource* ModuleResourceManager::LoadResourceIntoSlot(const uint32 uid, ResourceType type,
    const std::string& name, const std::string& assetsPath, const std::string& libraryPath)
{
	Resource* resource = InstantiateResource(type);
	if (!resource)
	{
		std::scoped_lock lock(resourcesMutex);
		resources.erase(uid);
		return nullptr;
	}

	resource->SetName(name);
	resource->SetUID(uid);
	resource->SetType(type);
	resource->SetAssetsPath(assetsPath);
	resource->SetLibraryPath(libraryPath);

	if (!mImporterManager->Deserialize(type, libraryPath, resource))
	{
		NOUS_ERROR("LoadResourceIntoSlot: failed to deserialize '%s' from '%s'", name.c_str(), libraryPath.c_str());
		DeleteResource(resource);
		std::scoped_lock lock(resourcesMutex);
		resources.erase(uid);
		return nullptr;
	}

	resource->SetState(ResourceState::CPU_READY);
	{ std::scoped_lock lock(resourcesMutex);        resources[uid] = resource; }
	{ std::scoped_lock lock(m_pendingUploadsMutex); m_pendingUploads.emplace_back(type, resource); }
	resource->IncreaseReferenceCount();
	resource->Validate();
	return resource;
}


Resource* ModuleResourceManager::CreateResource(const std::string& assetsPath)
{
	MetaFileData metaFileData;
	if (!ResourceImportPipeline::GetAssetMetaData(assetsPath, metaFileData))
	{
		NOUS_ERROR("CreateResource: failed to read meta for '%s'", assetsPath.c_str());
		return nullptr;
	}

	if (ClaimSlot(metaFileData.uid))
		return LoadResourceIntoSlot(metaFileData.uid, metaFileData.resourceType,
		    metaFileData.name, metaFileData.assetsPath, metaFileData.libraryPath);

	return SpinWaitForSlot(metaFileData.uid);
}

Resource* ModuleResourceManager::CreateResourceFromLibrary(const uint32 uid, const ResourceType type,
                                                            const std::string& name,
                                                            const std::string& assetsPath,
                                                            const std::string& libraryPath)
{
	if (ClaimSlot(uid))
		return LoadResourceIntoSlot(uid, type, name, assetsPath, libraryPath);

	return SpinWaitForSlot(uid);
}

ResourceMesh* ModuleResourceManager::RequestOrCreateSubMeshResourceFromLibrary(
    const std::string& libraryPath, const int32_t submeshIndex, const std::string& assetsPath)
{
	return m_subMeshCache.RequestOrCreateFromLibrary(libraryPath, submeshIndex, assetsPath);
}


bool ModuleResourceManager::UnloadResource(const uint32 uid)
{
	Resource* resource = nullptr;
	{
		std::lock_guard lock(resourcesMutex);
		const auto it = resources.find(uid);
		if (it == resources.end() || !it->second) return false; // not found or still loading (placeholder)
		resource = it->second;
		resource->DecreaseReferenceCount();
	}

	// Defer GPU Release + CPU Evict to the Renderer's PreUpdate so we never
	// destroy GPU resources mid-frame (between command recording and vkQueueSubmit).
	if (resource->GetReferenceCount() == 0)
	{
		std::lock_guard lock(m_pendingReleasesMutex);
		m_pendingReleases.emplace_back(resource->GetType(), resource);
	}

	return true;
}


void ModuleResourceManager::ClearResources(IGPUResourceFactory* gpu)
{
    // Common pattern for types that don't need special pre-destruction work:
    // filter by type → GPU destroy (if ready) → CPU Evict → DELETE.
    // Defined as a lambda so its body is a separate complexity unit.
    auto destroyByType = [&](const ResourceType type, const MemoryTag tag, auto gpuDestroy)
    {
        for (auto& res : resources | std::views::values)
        {
            if (!res || res->GetType() != type) continue;
            if (res->GetState() == ResourceState::GPU_READY)
                gpuDestroy(res);
            mImporterManager->Evict(type, res);
            NOUS_DELETE(res, tag);
        }
    };

    // Shaders first — descriptor sets reference texture image views;
    // freeing textures first would leave dangling view references in the sets.
    // VUID-vkDestroyImageView-01026
    destroyByType(ResourceType::SHADER, MemoryTag::RESOURCE_SHADER,
        [&gpu](Resource* r) { gpu->DestroyShader(down_cast<ResourceShader*>(r)); });

    // Materials next — destroy GPU handle, then clear texture pointers directly
    // (do NOT call UnloadResource; textures are destroyed in the next pass).
    for (auto& res : resources | std::views::values)
    {
        if (!res || res->GetType() != ResourceType::MATERIAL) continue;
        auto* mat = down_cast<ResourceMaterial*>(res);
        if (mat->GetState() == ResourceState::GPU_READY)
            gpu->DestroyMaterial(mat);
        for (auto& map : mat->textureMaps | std::views::values)
            map.texture = nullptr;
        NOUS_DELETE(res, MemoryTag::RESOURCE_MATERIAL);
    }

    destroyByType(ResourceType::TEXTURE, MemoryTag::RESOURCE_TEXTURE,
        [&gpu](Resource* r) { gpu->DestroyTexture(down_cast<ResourceTexture*>(r)); });

    destroyByType(ResourceType::MESH, MemoryTag::RESOURCE_MESH,
        [&gpu](Resource* r) { gpu->DestroyGeometry(down_cast<ResourceMesh*>(r)); });

    resources.clear();
    m_subMeshCache.Clear();

    m_builtinResources.Destroy(gpu);

    // Discard any stale queue entries — all resources are destroyed above.
    { std::lock_guard lock(m_pendingUploadsMutex);  m_pendingUploads.clear(); }
    { std::lock_guard lock(m_pendingReleasesMutex); m_pendingReleases.clear(); }
}

std::vector<std::pair<ResourceType, Resource*>> ModuleResourceManager::TakePendingUploads()
{
    std::vector<std::pair<ResourceType, Resource*>> result;
    std::scoped_lock lock(m_pendingUploadsMutex);
    std::swap(result, m_pendingUploads);
    return result;
}

std::vector<std::pair<ResourceType, Resource*>> ModuleResourceManager::TakePendingReleases()
{
    std::vector<std::pair<ResourceType, Resource*>> result;
    std::scoped_lock lock(m_pendingReleasesMutex);
    std::swap(result, m_pendingReleases);
    return result;
}

bool ModuleResourceManager::EvictResource(ResourceType type, Resource* resource)
{
    // Final refcount check + map erasure under the same lock so that a concurrent
    // CreateResource thread cannot bump the refcount between our check and the delete.
    {
        std::lock_guard lock(resourcesMutex);
        if (resource->GetReferenceCount() > 0)
        {
            // Re-acquired since TakePendingReleases. ImporterManager::Release already
            // freed the GPU resources, so re-queue for upload to restore GPU state.
            std::lock_guard uploadLock(m_pendingUploadsMutex);
            m_pendingUploads.emplace_back(type, resource);
            return false;
        }
        resources.erase(resource->GetUID());
    }

    mImporterManager->Evict(type, resource);
    DeleteResource(resource);
    return true;
}

ResourceTexture*  ModuleResourceManager::GetDefaultTexture()    const { return m_builtinResources.GetDefaultTexture();    }
ResourceTexture*  ModuleResourceManager::GetWhiteTexture()      const { return m_builtinResources.GetWhiteTexture();      }
ResourceTexture*  ModuleResourceManager::GetBlackTexture()      const { return m_builtinResources.GetBlackTexture();      }
ResourceTexture*  ModuleResourceManager::GetFlatNormalTexture() const { return m_builtinResources.GetFlatNormalTexture(); }
ResourceMaterial* ModuleResourceManager::GetDefaultMaterial()   const { return m_builtinResources.GetDefaultMaterial();  }

Resource* ModuleResourceManager::GetLoadedResource(const uint32 uid)
{
    std::lock_guard lock(resourcesMutex);
    const auto it = resources.find(uid);
    if (it == resources.end() || it->second == nullptr)
        return nullptr;
    return it->second;
}

IImporterManager* ModuleResourceManager::GetImporterManager() const
{
    return mImporterManager;
}


ResourceMesh* ModuleResourceManager::RequestOrCreateSubMeshResource(const std::string& assetsPath,
                                                                      const int32_t submeshIndex)
{
    return m_subMeshCache.RequestOrCreate(assetsPath, submeshIndex);
}

std::vector<std::future<void>> ModuleResourceManager::PreloadSceneResourcesAsync(
    nous::engine::multithreading::NOUS_JobSystem* jobSystem,
    const std::string& sceneFilePath)
{
    return m_scenePreloader.PreloadSceneResourcesAsync(jobSystem, sceneFilePath);
}