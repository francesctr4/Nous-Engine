#pragma once

#include "Engine/Modules/Module.h"
#include "Engine/EngineExport.h"
#include "Engine/Core/EventSystem/IEventListener.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Systems/ResourceManager/BuiltinResources/include/BuiltinResources.h"
#include "Engine/Systems/ResourceManager/ImportPipeline/include/ImportPipeline.h"
#include "Engine/Systems/ResourceManager/ScenePreloader/include/ScenePreloader.h"
#include "Engine/Systems/ResourceManager/SubMeshCache/include/SubMeshCache.h"
#include "Engine/Systems/ResourceManager/IResourceLoader.h"

#include "Engine/Core/FileWatcher/FileWatcher.h"

#include <atomic>
#include <future>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

class IGPUResourceFactory;
class IImporterManager;

class ResourceMesh;

class ResourceTexture;
class ResourceMaterial;

class ModuleResourceManager : public Module, public IEventListener, public IResourceLoader
{
public:

	// Constructor
	NOUS_ENGINE_API ModuleResourceManager(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem,
	                                      IImporterManager* importerManager);

	// Destructor
	~ModuleResourceManager() override;

	bool Awake() override;
	bool Start() override;

	UpdateStatus PreUpdate(float dt) override;
	UpdateStatus Update(float dt) override;
	UpdateStatus PostUpdate(float dt) override;

	bool CleanUp() override;

	void OnEvent(const Event& event) override;

	// ------------------------------------------------------------------------ //

	NOUS_ENGINE_API bool ImportFile(const std::string& path);
	NOUS_ENGINE_API bool ImportDirectory(const std::string& directory);

	// Editor-only: scans Assets/, imports everything, and mirrors scene files to Library/.
	// Called by Application::Awake() when not in game mode.
	NOUS_ENGINE_API void ScanAndImportAssets();

	// Editor-only: deletes all Library/ binaries, then runs a full reimport from Assets/.
	// .meta sidecars are preserved so UIDs remain stable.
	NOUS_ENGINE_API void RegenerateLibrary();

	NOUS_ENGINE_API bool ResourceExists(uint32 uid) const;
	NOUS_ENGINE_API Resource* CreateResource(const std::string& assetsPath) override;

	// GAME mode variant: load directly from a known library path without reading a .meta file.
	NOUS_ENGINE_API Resource* CreateResourceFromLibrary(uint32 uid, ResourceType type,
	                                                    const std::string& name,
	                                                    const std::string& assetsPath,
	                                                    const std::string& libraryPath) override;

	// GAME mode variant: load a submesh by library path + index, no .meta required.
	// `assetsPath` is optional: when provided (EDITOR path), it is stamped onto the
	// created resource so later serialization (Scene snapshot, Save Scene) can write
	// a non-empty assetPath back out. Without it the resource would carry an empty
	// assetsPath and CMesh::Deserialize would drop the reference on the next load.
	NOUS_ENGINE_API ResourceMesh* RequestOrCreateSubMeshResourceFromLibrary(
	    const std::string& libraryPath, int32_t submeshIndex,
	    const std::string& assetsPath, uint32 hintUID = 0) override;

	NOUS_ENGINE_API bool UnloadResource(uint32 uid);

	// Update the in-memory assets path of a loaded resource after it has been moved on disk.
	// No-op if the UID is not currently loaded.
	NOUS_ENGINE_API void UpdateResourcePath(uint32 uid, const std::string& newAssetsPath);

	// Returns a thread-safe snapshot copy of the resources map.
	// Safe to call from any thread (e.g. editor UI) concurrently with resource loading.
	NOUS_ENGINE_API std::unordered_map<uint32, Resource*> GetResourcesMap() const;

	// Takes and clears the pending upload queue — called by Renderer::PreUpdate/Start.
	// Each entry is a resource that has been Deserialized and needs GPU Upload.
	NOUS_ENGINE_API std::vector<std::pair<ResourceType, Resource*>> TakePendingUploads();

	// Takes and clears the pending release queue — called by Renderer::PreUpdate.
	// Each entry is a resource whose ref count hit 0 and needs GPU Release + CPU Evict.
	NOUS_ENGINE_API std::vector<std::pair<ResourceType, Resource*>> TakePendingReleases();

	// Called by Renderer after GPU Release: evicts CPU data and deletes the resource object.
	// Returns true if the resource was evicted, false if it was re-acquired (and re-queued for upload).
	NOUS_ENGINE_API bool EvictResource(ResourceType type, Resource* resource);

	struct PendingAssetUpload { uint32 uid; ResourceType type; };

	// Hot reload — takes and clears the queue of reimported resources awaiting GPU upload.
	// Called by ModuleRenderer3D::PreUpdate() each frame.
	NOUS_ENGINE_API std::vector<PendingAssetUpload> TakeReadyAssetUploads();

	// Call once (from Application) when running in game/standalone mode to suppress
	// asset watching, which is editor-only.
	NOUS_ENGINE_API void SetGameMode() noexcept;

	// Dispatches an async reimport job for a tracked asset path.
	// Public to allow direct testing; normally called only by the FileWatcher callback.
	NOUS_ENGINE_API void DispatchReimportJob(const std::string& normalizedPath);

	// Synchronous full teardown — caller must pass the GPU factory so GPU handles can
	// be freed before Vulkan is shut down.  Only called from ModuleRenderer3D::CleanUp().
	NOUS_ENGINE_API void ClearResources(IGPUResourceFactory* gpu);

    [[nodiscard]] ResourceTexture* GetDefaultTexture() const;
    [[nodiscard]] ResourceTexture* GetWhiteTexture() const;
    [[nodiscard]] ResourceTexture* GetBlackTexture() const;
    [[nodiscard]] ResourceTexture* GetFlatNormalTexture() const;
    [[nodiscard]] NOUS_ENGINE_API ResourceMaterial* GetDefaultMaterial() const;

    // Returns the resource pointer WITHOUT bumping the reference count (borrowed reference).
    // Use for read-only access (e.g. Inspector UI) where the caller does not own the resource.
    // Do NOT call UnloadResource on the returned pointer.
    // Returns nullptr if the resource is not currently loaded.
    NOUS_ENGINE_API Resource* GetLoadedResource(uint32 uid);

    // Returns the injected importer manager — used by ModuleRenderer3D to call
    // Upload/Release through the IImporterManager interface.
    NOUS_ENGINE_API IImporterManager* GetImporterManager() const;

    // Returns the ResourceMesh for a specific submesh within a source asset.
    // If already loaded this session, bumps the ref count and returns it.
    // Otherwise loads the submesh from the library binary, uploads it to the GPU,
    // and registers it in the resource map with a generated UID.
    NOUS_ENGINE_API ResourceMesh* RequestOrCreateSubMeshResource(const std::string& assetsPath,
                                                                  int32_t submeshIndex) override;

    // Scans a .nous scene file for all CMesh resource requests and submits one parallel
    // Deserialize job per unique (assetPath, submeshIndex) pair.
    // Returns futures — wait on all before calling Scene::Deserialize() so CMesh::Deserialize()
    // hits the resource cache instead of blocking on disk I/O.
    NOUS_ENGINE_API std::vector<std::future<void>> PreloadSceneResourcesAsync(
        nous::engine::multithreading::NOUS_JobSystem* jobSystem,
        const std::string& sceneFilePath);

private:

	void DeleteResource(Resource*& resource);

	// Atomically claims the UID slot with a nullptr placeholder.
	// Returns true if this thread won the race and must load; false if another thread already owns it.
	bool ClaimSlot(uint32 uid);

	// Spins until the slot's loading thread writes a real pointer, then bumps the refcount and returns it.
	// Returns nullptr if the entry was evicted from the map before it resolved.
	Resource* SpinWaitForSlot(uint32 uid);

	// Instantiates, populates, deserializes, and registers a resource into an already-claimed slot.
	// Caller must have won ClaimSlot() before calling this.
	Resource* LoadResourceIntoSlot(uint32 uid, ResourceType type,
	    const std::string& name,
	    const std::string& assetsPath,
	    const std::string& libraryPath);

	mutable std::mutex resourcesMutex;  // mutable: const methods (e.g. GetResourcesMap) can lock it
	std::unordered_map<uint32, Resource*> resources;

	// Resources waiting for GPU upload — populated by CreateResource/Deserialize paths.
	// Drained by Renderer::PreUpdate (and Start for the initial set).
	std::mutex m_pendingUploadsMutex;
	std::vector<std::pair<ResourceType, Resource*>> m_pendingUploads;

	// Resources whose refcount hit 0 — waiting for GPU Release then CPU Evict.
	// Drained by Renderer::PreUpdate each frame.
	std::mutex m_pendingReleasesMutex;
	std::vector<std::pair<ResourceType, Resource*>> m_pendingReleases;

	// Injected dependencies
	IImporterManager*       mImporterManager = nullptr;
	BuiltinResources        m_builtinResources;
	ImportPipeline  m_importPipeline;
	ScenePreloader  m_scenePreloader;

	// --- Asset hot reload (EDITOR only) ---
	FileWatcher                               m_assetWatcher;
	std::unordered_map<std::string, uint32>   m_pathToUID;         // normalized path → UID; protected by resourcesMutex
	std::mutex                                m_reimportMutex;
	std::unordered_set<uint32>                m_inFlightUIDs;       // protected by m_reimportMutex
	std::atomic<int>                          m_inFlightJobCount{0};
	std::mutex                                m_uploadQueueMutex;
	std::vector<PendingAssetUpload>           m_readyUploads;       // worker → main thread handoff
	bool                                      m_isEditorMode = true;
	uint32_t                                  m_assetWatchFrameCounter = 0;
	std::string                               m_sourceAssetsDir;  // absolute path to source Assets/ (editor only)

	// Worker-thread callers (e.g. scene preload) push assetsPaths here; the main
	// thread drains it in PreUpdate before Poll() and registers each with the
	// (single-threaded) FileWatcher.
	std::mutex                                m_watchRegistrationMutex;
	std::vector<std::string>                  m_pendingWatchRegistrations;

	// Declared after m_pathToUID/m_pendingWatchRegistrations/m_watchRegistrationMutex so
	// those are initialized first (SubMeshCache stores references to them).
	SubMeshCache            m_subMeshCache;

	static std::string NormalizeAssetPath(const std::string& path);

	// Walks up from the current working directory until finding a directory that
	// has both Assets/ and Source/ children — that is the project root.
	// Returns the absolute forward-slash path to its Assets/ child, or empty if not found.
	static std::string FindSourceAssetsDir();

	// Scans the source Assets/ directory and registers any unregistered files with
	// m_assetWatcher. Safe to call repeatedly — uses WatchIfNew under the hood.
	void ScanAndRegisterWatchedAssets();

	// Registers a single asset with the FileWatcher for hot reload, mirroring the
	// path layout used by ScanAndRegisterWatchedAssets. `assetsPath` is the
	// engine-relative path (e.g. "Assets/Foo/bar.png"). No-op outside editor mode
	// or if m_sourceAssetsDir could not be resolved.
	void RegisterAssetForHotReload(const std::string& assetsPath);

};
