#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include <ResourceManager/Core/ResourceBase.h>
#include "Engine/Core/EventSystem/EventSystem.h"
#include <ResourceManager/Types/ResourceMesh/ResourceMesh.h>
#include <ResourceManager/Types/ResourceMaterial/ResourceMaterial.h>
#include <ResourceManager/Types/ResourceTexture/ResourceTexture.h>
#include <ResourceManager/Types/ResourceShader/ResourceShader.h>
#include <ResourceManager/Types/ResourceAudio/ResourceAudio.h>
#include <Renderer/IGPUResourceFactory.h>
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include <Logger/Logger.h>
#include <MemoryManager/MemoryManager.h>

#include <ResourceManager/Core/MetaFileData.h>

#include <ResourceManager/Core/IImporterManager.h>
#include <ResourceManager/Core/IImporter.h>
#include <ResourceManager/Core/TypeRegistry.h>
#include <NOUS_Multithreading/NOUS_Thread.h>
#include <NOUS_Multithreading/NOUS_JobSystem.h>

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
    ResourceBase* InstantiateResource(const TypeRegistry& reg, const ResourceType type)
    {
        const TypeDescriptor* d = reg.Get(type);
        return (d && d->createFn) ? d->createFn(0) : nullptr;
    }
}

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_RESOURCEMANAGER;

ModuleResourceManager::ModuleResourceManager(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem,
                                             IImporterManager* importerManager, const TypeRegistry& typeRegistry)
    : Module(eventSystem, jobSystem)
    , mImporterManager(importerManager)
    , mTypeRegistry(&typeRegistry)
    , m_importPipeline(importerManager, typeRegistry, jobSystem)
    , m_scenePreloader(this)
    , m_hotReloader(importerManager, typeRegistry, jobSystem)
    , m_subMeshCache(m_table, m_pendingUploads)
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
	m_pendingUploads.PushBatch(m_builtinResources.Create());

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

void ModuleResourceManager::RefreshSceneManifest()
{
	ImportPipeline::WriteSceneManifest();
}

std::unordered_map<uint32, ResourceBase*> ModuleResourceManager::GetResourcesMap() const
{
	return m_table.Snapshot();
}


// Iterating a snapshot rather than under ScopedLock is required, not incidental:
// the renderer's callbacks dispatch shader compile jobs and re-acquire descriptor
// slots (which re-enters the resource system), so holding the table's
// non-recursive mutex across them would deadlock.
void ModuleResourceManager::ForEachShader(const std::function<void(ResourceShader*)>& fn) const
{
	const auto snapshot = m_table.Snapshot();

	for (ResourceBase* resource : snapshot | std::views::values)
	{
		// A claimed-but-still-loading slot holds a nullptr placeholder.
		if (resource == nullptr || resource->GetType() != ResourceType::SHADER)
			continue;

		fn(down_cast<ResourceShader*>(resource));
	}
}


void ModuleResourceManager::ForEachMaterial(const std::function<void(ResourceMaterial*)>& fn) const
{
	const auto snapshot = m_table.Snapshot();

	for (ResourceBase* resource : snapshot | std::views::values)
	{
		if (resource == nullptr || resource->GetType() != ResourceType::MATERIAL)
			continue;

		fn(down_cast<ResourceMaterial*>(resource));
	}
}


void ModuleResourceManager::DeleteResource(ResourceBase*& resource)
{
	// SubMeshCache::EraseUID requires the table's lock to be held by the caller.
	{
		ResourceTable::ScopedLock lock(m_table);
		m_subMeshCache.EraseUID(resource->GetUID());
	}

	if (const TypeDescriptor* d = mTypeRegistry->Get(resource->GetType());
		d && d->destroyFn)
		d->destroyFn(resource);

	// NOTE: m_table erasure is intentionally NOT here.
	// EvictResource removes the entry under the table lock before calling DeleteResource,
	// so the map is clean before we reach this point.
}

bool ModuleResourceManager::ResourceExists(const uint32 uid) const
{
	return m_table.Contains(uid);
}

void ModuleResourceManager::UpdateResourcePath(const uint32 uid, const std::string& newAssetsPath)
{
	ResourceTable::ScopedLock lock(m_table);
	auto& resources = lock.Map();
	auto it = resources.find(uid);
	if (it != resources.end())
		it->second->SetAssetsPath(newAssetsPath);
}

ResourceBase* ModuleResourceManager::SpinWaitForSlot(const uint32 uid)
{
	ResourceBase* resource = nullptr;
	while (true)
	{
		{
			ResourceTable::ScopedLock lock(m_table);
			auto& resources = lock.Map();
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
	return resource;
}

ResourceBase* ModuleResourceManager::LoadResourceIntoSlot(const uint32 uid, ResourceType type,
    const std::string& name, const std::string& assetsPath, const std::string& libraryPath)
{
	ResourceBase* resource = InstantiateResource(*mTypeRegistry, type);
	if (!resource)
	{
		m_table.Erase(uid);
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
		m_table.Erase(uid);
		return nullptr;
	}

	resource->SetState(ResourceState::CPU_READY);
	m_table.Set(uid, resource);

	// HotReloader filters by type/enabled/empty-path internally — safe to call
	// for every load; it no-ops when not applicable.
	m_hotReloader.TrackAsset(assetsPath, uid, type);
	m_pendingUploads.Push(type, resource);
	resource->IncreaseReferenceCount();
	return resource;
}


ResourceBase* ModuleResourceManager::CreateResource(const std::string& assetsPath)
{
	MetaFileData metaFileData;
	if (!ImportPipeline::GetAssetMetaData(assetsPath, metaFileData))
	{
		NOUS_ERROR("CreateResource: failed to read meta for '%s'", assetsPath.c_str());
		return nullptr;
	}

	if (m_table.TryInsert(metaFileData.uid, nullptr))
		return LoadResourceIntoSlot(metaFileData.uid, metaFileData.resourceType,
		    metaFileData.name, metaFileData.assetsPath, metaFileData.libraryPath);

	return SpinWaitForSlot(metaFileData.uid);
}

ResourceBase* ModuleResourceManager::CreateResourceFromLibrary(const uint32 uid, const ResourceType type,
                                                            const std::string& name,
                                                            const std::string& assetsPath,
                                                            const std::string& libraryPath)
{
	if (m_table.TryInsert(uid, nullptr))
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
	ResourceBase* resource = nullptr;
	{
		ResourceTable::ScopedLock lock(m_table);
		auto& resources = lock.Map();
		const auto it = resources.find(uid);
		if (it == resources.end() || !it->second) return false; // not found or still loading (placeholder)
		resource = it->second;
		resource->DecreaseReferenceCount();
	}

	// Defer GPU Release + CPU Evict to the Renderer's PreUpdate so we never
	// destroy GPU resources mid-frame (between command recording and vkQueueSubmit).
	if (resource->GetReferenceCount() == 0)
		m_pendingReleases.Push(resource->GetType(), resource);

	return true;
}


void ModuleResourceManager::ClearResources(IGPUResourceFactory* gpu)
{
    // Single-threaded shutdown by contract; access the raw map directly.
    ResourceTable::ScopedLock lock(m_table);
    auto& resources = lock.Map();

    // Tear down in registry-defined cleanup priority order. Shaders go first
    // (descriptor sets reference texture image views; freeing textures first
    // would leave dangling references — VUID-vkDestroyImageView-01026), then
    // materials, then leaf resources.
    for (const TypeDescriptor* d : mTypeRegistry->SortedByCleanupPriority())
    {
        for (auto& res : resources | std::views::values)
        {
            if (!res || res->GetType() != d->type) continue;

            // GPU teardown (importer guards its own internal IDs). Pipeline-only
            // types have no resourceImporter — nothing to release.
            if (res->GetState() == ResourceState::GPU_READY && d->resourceImporter)
                d->resourceImporter->Release(res, gpu);

            // Per-type teardown: descriptors that hold peer-resource pointers
            // (e.g. Material -> Texture) supply an inline cleanup callback to
            // avoid recursing through UnloadResource into the half-torn-down
            // manager. Everything else takes the normal Evict path.
            if (d->evictAtShutdown)
                d->evictAtShutdown(res);
            else if (d->resourceImporter)
                d->resourceImporter->Evict(res);

            if (d->destroyFn) d->destroyFn(res);
        }
    }

    resources.clear();
    m_subMeshCache.Clear();

    m_builtinResources.Destroy(gpu);

    // Discard any stale queue entries — all resources are destroyed above.
    m_pendingUploads.Clear();
    m_pendingReleases.Clear();
}

std::vector<std::pair<ResourceType, ResourceBase*>> ModuleResourceManager::TakePendingUploads()
{
    return m_pendingUploads.TakeAll();
}

std::vector<std::pair<ResourceType, ResourceBase*>> ModuleResourceManager::TakePendingReleases()
{
    return m_pendingReleases.TakeAll();
}

bool ModuleResourceManager::EvictResource(ResourceType type, ResourceBase* resource)
{
    // Final refcount check + map erasure under the same lock so that a concurrent
    // CreateResource thread cannot bump the refcount between our check and the delete.
    std::string evictedAssetsPath;
    {
        ResourceTable::ScopedLock lock(m_table);
        if (resource->GetReferenceCount() > 0)
        {
            // Re-acquired since TakePendingReleases. ImporterManager::Release already
            // freed the GPU resources, so re-queue for upload to restore GPU state.
            // ResourceQueue locks internally — safe to call with table lock held;
            // the nesting order (table → uploads) is the only one used in this module.
            m_pendingUploads.Push(type, resource);
            return false;
        }
        evictedAssetsPath = resource->GetAssetsPath();
        lock.Map().erase(resource->GetUID());
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

ResourceBase* ModuleResourceManager::GetLoadedResource(const uint32 uid)
{
    return m_table.TryGet(uid);
}

IImporterManager* ModuleResourceManager::GetImporterManager() const
{
    return mImporterManager;
}

const TypeRegistry& ModuleResourceManager::GetTypeRegistry() const
{
    return *mTypeRegistry;
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
