#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Core/Resource/include/Resource.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Types/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Systems/ResourceManager/Types/ResourceShader/include/ResourceShader.h"
#include "Engine/Systems/ResourceManager/Types/ResourceAudio/include/ResourceAudio.h"
#include "Engine/Renderer/IGPUResourceFactory.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Systems/ResourceManager/Core/Resource/include/MetaFileData.h"

#include "Engine/Systems/ResourceManager/Core/ImporterManager/include/IImporterManager.h"
#include "Engine/Systems/ResourceManager/Core/ImporterManager/include/IImporter.h"
#include "Engine/Systems/ResourceManager/Core/TypeRegistry/include/TypeRegistry.h"
#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"

#include <future>
#include <ranges>
#include <utility>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Resource lifecycle is routed through the TypeRegistry — adding a
// new type is now a single descriptor block in RegisterResourceTypes.
// ---------------------------------------------------------------------------
namespace
{
    Resource* InstantiateResource(const ResourceType type)
    {
        const TypeDescriptor* d = GetTypeRegistry().Get(type);
        return (d && d->createFn) ? d->createFn(0) : nullptr;
    }
}

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_RESOURCEMANAGER;

ModuleResourceManager::ModuleResourceManager(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem,
                                             IImporterManager* importerManager)
    : Module(eventSystem, jobSystem)
    , mImporterManager(importerManager)
    , m_importPipeline(importerManager, jobSystem)
    , m_scenePreloader(this)
    , m_hotReloader(importerManager, jobSystem)
    , m_subMeshCache(resources, resourcesMutex, m_pendingUploads, m_pendingUploadsMutex)
{
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

void ModuleResourceManager::RegenerateLibrary()
{
	m_importPipeline.ClearLibraryFiles();
	// This runs on a job-system worker. Sub-jobs can only run in parallel if other
	// workers are free — with only 1 worker the queue would starve waiting on the latch.
	const bool canParallelize = JobSystem && JobSystem->GetWorkerCount() > 1;
	m_importPipeline.ScanAndImportAssets(canParallelize);
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

	m_hotReloader.Start();

	return true;
}

UpdateStatus ModuleResourceManager::PreUpdate(float dt)
{
	m_hotReloader.Poll();
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
	// Worker lambdas capture HotReloader's `this` and write into its ready queue
	// — letting them run after teardown would be a use-after-free.
	m_hotReloader.WaitForInFlight();
	return true;
}

void ModuleResourceManager::OnEvent(const Event& event)
{
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

	if (const TypeDescriptor* d = GetTypeRegistry().Get(resource->GetType());
		d && d->destroyFn)
		d->destroyFn(resource);

	// NOTE: resources.erase(uid) is intentionally NOT here.
	// EvictResource removes the entry under resourcesMutex before calling DeleteResource,
	// so the map is clean before we reach this point.
}

bool ModuleResourceManager::ResourceExists(const uint32 uid) const
{
	std::scoped_lock lock(resourcesMutex);
	return resources.contains(uid);
}

void ModuleResourceManager::UpdateResourcePath(const uint32 uid, const std::string& newAssetsPath)
{
	std::lock_guard lock(resourcesMutex);
	auto it = resources.find(uid);
	if (it != resources.end())
		it->second->SetAssetsPath(newAssetsPath);
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
	{
		std::scoped_lock lock(resourcesMutex);
		resources[uid] = resource;
	}
	// HotReloader filters by type/enabled/empty-path internally — safe to call
	// for every load; it no-ops when not applicable.
	m_hotReloader.TrackAsset(assetsPath, uid, type);
	{ std::scoped_lock lock(m_pendingUploadsMutex); m_pendingUploads.emplace_back(type, resource); }
	resource->IncreaseReferenceCount();
	resource->Validate();
	return resource;
}


Resource* ModuleResourceManager::CreateResource(const std::string& assetsPath)
{
	MetaFileData metaFileData;
	if (!ImportPipeline::GetAssetMetaData(assetsPath, metaFileData))
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
    const std::string& libraryPath, const int32_t submeshIndex,
    const std::string& assetsPath, const uint32 hintUID)
{
	return m_subMeshCache.RequestOrCreateFromLibrary(libraryPath, submeshIndex, assetsPath, hintUID);
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
    // Tear down in registry-defined cleanup priority order. Shaders go first
    // (descriptor sets reference texture image views; freeing textures first
    // would leave dangling references — VUID-vkDestroyImageView-01026), then
    // materials, then leaf resources.
    for (const TypeDescriptor* d : GetTypeRegistry().SortedByCleanupPriority())
    {
        for (auto& res : resources | std::views::values)
        {
            if (!res || res->GetType() != d->type) continue;

            // GPU teardown (importer guards its own internal IDs).
            if (res->GetState() == ResourceState::GPU_READY && d->importer)
                d->importer->Release(res, gpu);

            // MATERIAL is special at shutdown: textures are still alive (lower
            // priority — destroyed in a later pass). Null the pointers inline;
            // do NOT route through Evict, which would call UnloadResource and
            // recurse into a half-torn-down manager.
            if (d->type == ResourceType::MATERIAL)
            {
                auto* mat = down_cast<ResourceMaterial*>(res);
                for (auto& map : mat->textureMaps | std::views::values)
                    map.texture = nullptr;
            }
            else if (d->importer)
            {
                d->importer->Evict(res);
            }

            if (d->destroyFn) d->destroyFn(res);
        }
    }

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
    std::string evictedAssetsPath;
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
        evictedAssetsPath = resource->GetAssetsPath();
        resources.erase(resource->GetUID());
    }

    m_hotReloader.UntrackAsset(evictedAssetsPath);
    mImporterManager->Evict(type, resource);
    DeleteResource(resource);
    return true;
}

void ModuleResourceManager::SetGameMode() noexcept
{
    m_hotReloader.Disable();
}

std::vector<ModuleResourceManager::PendingAssetUpload> ModuleResourceManager::TakeReadyAssetUploads()
{
    return m_hotReloader.TakeReadyUploads();
}

void ModuleResourceManager::DispatchReimportJob(const std::string& normalizedPath)
{
    m_hotReloader.DispatchReimportJob(normalizedPath);
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
