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
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"

#include <future>
#include <ranges>
#include <thread>
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

/*static*/ std::string ModuleResourceManager::NormalizeAssetPath(const std::string& path)
{
    std::string result = path;
    std::replace(result.begin(), result.end(), '\\', '/');
    return result;
}

/*static*/ std::string ModuleResourceManager::FindSourceAssetsDir()
{
    namespace fs = std::filesystem;
    std::error_code ec;
    fs::path dir = fs::current_path(ec);
    if (ec) return {};

    for (int depth = 0; depth < 8; ++depth)
    {
        if (fs::exists(dir / "Assets", ec) && fs::exists(dir / "Source", ec))
            return NormalizeAssetPath((dir / "Assets").generic_string());
        fs::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = std::move(parent);
    }
    return {};
}

ModuleResourceManager::ModuleResourceManager(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem,
                                             IImporterManager* importerManager)
    : Module(eventSystem, jobSystem)
    , mImporterManager(importerManager)
    , m_importPipeline(importerManager, jobSystem)
    , m_scenePreloader(this)
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

	if (m_isEditorMode)
	{
		m_sourceAssetsDir = FindSourceAssetsDir();
		ScanAndRegisterWatchedAssets();
	}

	return true;
}


void ModuleResourceManager::ScanAndRegisterWatchedAssets()
{
	namespace fs = std::filesystem;
	std::error_code ec;
	int watchCount = 0;

	const std::string watchBase = m_sourceAssetsDir.empty() ? "Assets" : m_sourceAssetsDir;
	const fs::path sourceRoot = m_sourceAssetsDir.empty()
	    ? fs::path{}
	    : fs::path(m_sourceAssetsDir).parent_path();

	for (auto& entry : fs::recursive_directory_iterator(watchBase, ec))
	{
		if (!entry.is_regular_file()) continue;
		const auto ext = entry.path().extension();
		if (ext == ".meta" || ext == ".nous") continue;
		if (ext == ".glsl") continue;
		if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb" || ext == ".dae") continue;

		const std::string watchPath = NormalizeAssetPath(entry.path().generic_string());

		std::string relKey;
		if (!m_sourceAssetsDir.empty())
			relKey = NormalizeAssetPath(fs::relative(entry.path(), sourceRoot, ec).generic_string());
		else
			relKey = watchPath;

		// WatchIfNew (not Watch) so a periodic rescan does not reset the baseline
		// mtime of a file that already has a pending modification.
		m_assetWatcher.WatchIfNew(watchPath, [this, relKey](const std::string&)
		{
			NOUS_INFO_C(CURRENT_CHANNEL, "[AssetHotReload] FileWatcher fired for: %s", relKey.c_str());
			DispatchReimportJob(relKey);
		});
		++watchCount;
	}

	if (ec)
		NOUS_WARN_C(CURRENT_CHANNEL, "[AssetHotReload] Could not scan for asset watching: %s", ec.message().c_str());
}

void ModuleResourceManager::RegisterAssetForHotReload(const std::string& assetsPath)
{
	if (!m_isEditorMode || m_sourceAssetsDir.empty() || assetsPath.empty())
		return;

	// Filter to types we hot-reload and skip sidecars.
	const auto extPos = assetsPath.find_last_of('.');
	if (extPos == std::string::npos) return;
	const std::string ext = assetsPath.substr(extPos);
	if (ext == ".meta" || ext == ".nous" || ext == ".glsl") return;
	if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb" || ext == ".dae") return;

	const std::string relKey = NormalizeAssetPath(assetsPath);

	// Map "Assets/foo/bar.png" → "<sourceAssetsDir>/foo/bar.png".
	// m_sourceAssetsDir already ends in "/Assets" (no trailing slash).
	std::string watchPath;
	if (relKey.size() > 7 && relKey.substr(0, 7) == "Assets/")
		watchPath = m_sourceAssetsDir + "/" + relKey.substr(7);
	else
		watchPath = relKey;

	m_assetWatcher.WatchIfNew(watchPath, [this, relKey](const std::string&)
	{
		NOUS_INFO_C(CURRENT_CHANNEL, "[AssetHotReload] FileWatcher fired for: %s", relKey.c_str());
		DispatchReimportJob(relKey);
	});
}

UpdateStatus ModuleResourceManager::PreUpdate(float dt)
{
	if (m_isEditorMode)
	{
		// Drain any pending watch registrations queued from worker threads
		// (e.g. scene preload) or from CreateResource on the main thread.
		std::vector<std::string> pending;
		{
			std::scoped_lock lock(m_watchRegistrationMutex);
			std::swap(pending, m_pendingWatchRegistrations);
		}
		for (const auto& assetsPath : pending)
			RegisterAssetForHotReload(assetsPath);

		++m_assetWatchFrameCounter;
		if (m_assetWatchFrameCounter % 30 == 0)
			m_assetWatcher.Poll();
	}

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
	// Wait for any in-flight reimport jobs to finish before tearing down.
	// Worker lambdas capture `this` and write into m_readyUploads — letting them
	// run after CleanUp returns would be a use-after-free.
	while (m_inFlightJobCount.load(std::memory_order_acquire) > 0)
		std::this_thread::yield();

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
	bool registerForHotReload = false;
	{
		std::scoped_lock lock(resourcesMutex);
		resources[uid] = resource;
		if ((type == ResourceType::TEXTURE || type == ResourceType::MATERIAL)
		    && !assetsPath.empty())
		{
			m_pathToUID[NormalizeAssetPath(assetsPath)] = uid;
			registerForHotReload = true;
		}
	}
	// FileWatcher is single-threaded; LoadResourceIntoSlot may run on a worker
	// (scene preload). Queue the assetsPath; PreUpdate drains it on the main thread.
	if (registerForHotReload && m_isEditorMode)
	{
		std::scoped_lock lock(m_watchRegistrationMutex);
		m_pendingWatchRegistrations.push_back(assetsPath);
	}
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
        m_pathToUID.erase(NormalizeAssetPath(resource->GetAssetsPath()));
    }

    mImporterManager->Evict(type, resource);
    DeleteResource(resource);
    return true;
}

void ModuleResourceManager::SetGameMode() noexcept
{
    m_isEditorMode = false;
}

std::vector<ModuleResourceManager::PendingAssetUpload> ModuleResourceManager::TakeReadyAssetUploads()
{
    std::vector<PendingAssetUpload> result;
    std::lock_guard lock(m_uploadQueueMutex);
    std::swap(result, m_readyUploads);
    return result;
}

void ModuleResourceManager::DispatchReimportJob(const std::string& normalizedPath)
{
    NOUS_INFO_C(CURRENT_CHANNEL, "[AssetHotReload] DispatchReimportJob called for: %s", normalizedPath.c_str());

    // Resolve path → resource under resourcesMutex so the lookup and pointer capture are atomic.
    uint32       uid = 0;
    Resource*    resource    = nullptr;
    ResourceType type        = ResourceType::UNKNOWN;
    std::string  assetsPath;
    std::string  libraryPath;
    {
        std::lock_guard lock(resourcesMutex);
        const auto pathIt = m_pathToUID.find(normalizedPath);
        if (pathIt == m_pathToUID.end())
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "[AssetHotReload] '%s' not in m_pathToUID (%zu entries tracked).",
                        normalizedPath.c_str(), m_pathToUID.size());
            for (const auto& [p, id] : m_pathToUID)
                NOUS_INFO_C(CURRENT_CHANNEL, "[AssetHotReload]   tracked: %s (uid=%u)", p.c_str(), id);
            return;
        }
        uid = pathIt->second;
        const auto resIt = resources.find(uid);
        if (resIt == resources.end() || !resIt->second) return;   // evicted between watcher fire and dispatch
        resource    = resIt->second;
        type        = resource->GetType();
        assetsPath  = resource->GetAssetsPath();
        libraryPath = resource->GetLibraryPath();
    }

    // Dedup: at most one in-flight job per asset UID at a time.
    {
        std::lock_guard lock(m_reimportMutex);
        if (!m_inFlightUIDs.insert(uid).second) return;
    }
    ++m_inFlightJobCount;

    // Capture source dir so the job lambda can redirect Import to the source file.
    const std::string sourceAssetsDir = m_sourceAssetsDir;

    JobSystem->SubmitJob([this, uid, resource, type, assetsPath, libraryPath, sourceAssetsDir]
    {
        auto cleanup = [this, uid]
        {
            { std::lock_guard lock(m_reimportMutex); m_inFlightUIDs.erase(uid); }
            --m_inFlightJobCount;
        };

        // 1. Read .meta sidecar to get confirmed library path / type.
        MetaFileData metaData;
        if (!ResourceImportPipeline::GetAssetMetaData(assetsPath, metaData))
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "[AssetHotReload] No .meta found for '%s' — skipping reimport.", assetsPath.c_str());
            cleanup(); return;
        }

        // Redirect Import to the source file so it reads what the user actually edited,
        // not the stale configure-time binary copy (e.g. Build/bin/Assets/...).
        // assetsPath is like "Assets/foo/bar.nmat"; sourceAssetsDir ends with "/Assets".
        if (!sourceAssetsDir.empty() && assetsPath.size() > 7
            && assetsPath.substr(0, 7) == "Assets/")
        {
            metaData.assetsPath = sourceAssetsDir + "/" + assetsPath.substr(7);
        }

        // 2. Reimport source → Library binary (CPU only, no Vulkan).
        if (!mImporterManager->Import(type, metaData))
        {
            NOUS_WARN_C(CURRENT_CHANNEL, "[AssetHotReload] Import failed for '%s' — old asset kept.", assetsPath.c_str());
            cleanup(); return;
        }

        // 3. Hand off to the main thread for Deserialize + GPU Release + Upload.
        // Deserialize must NOT run here: it mutates live resource fields (e.g. poolOwnerShader via
        // SetShader) that the main thread reads concurrently in DrawGeometryBatched, causing a crash.
        { std::lock_guard lock(m_uploadQueueMutex); m_readyUploads.push_back({uid, type}); }
        NOUS_INFO_C(CURRENT_CHANNEL, "[AssetHotReload] Queued GPU upload for '%s'.", assetsPath.c_str());

        cleanup();
    }, "ReimportAsset");
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