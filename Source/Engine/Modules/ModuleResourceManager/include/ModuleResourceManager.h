#pragma once

#include "Engine/Modules/Module.h"
#include "Engine/EngineExport.h"
#include "Engine/Core/EventSystem/IEventListener.h"
#include "Engine/Systems/ResourceManager/Core/ResourceBase/include/ResourceBase.h"
#include "Engine/Systems/ResourceManager/Core/ResourceQueue/include/ResourceQueue.h"
#include "Engine/Systems/ResourceManager/Core/ResourceTable/include/ResourceTable.h"
#include "Engine/Systems/ResourceManager/Runtime/Builtins/include/BuiltinResources.h"
#include "Engine/Systems/ResourceManager/Runtime/HotReloader/include/HotReloader.h"
#include "Engine/Systems/ResourceManager/Runtime/ImportPipeline/include/ImportPipeline.h"
#include "Engine/Systems/ResourceManager/Runtime/ScenePreloader/include/ScenePreloader.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMesh/include/SubMeshCache.h"
#include "Engine/Systems/ResourceManager/Core/ResourceBase/include/IResourceLoader.h"

#include <future>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class IGPUResourceFactory;
class IImporterManager;
class TypeRegistry;

class ResourceMesh;

class ResourceTexture;
class ResourceMaterial;

class ModuleResourceManager : public Module, public IEventListener, public IResourceLoader
{
public:

	// Constructor
	NOUS_ENGINE_API ModuleResourceManager(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem,
	                                      IImporterManager* importerManager, const TypeRegistry& typeRegistry);

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

	// Rebuilds Library/Scenes/scene_manifest.json from current scene .meta files.
	// Called after a single-scene save so GAME mode sees it without a full rescan.
	NOUS_ENGINE_API void RefreshSceneManifest();

	// Editor-only: scans Assets/, imports everything, and mirrors scene files to Library/.
	// Called by Application::Awake() when not in game mode.
	NOUS_ENGINE_API void ScanAndImportAssets();

	// Editor-only: deletes all Library/ binaries, then runs a full reimport from Assets/.
	// .meta sidecars are preserved so UIDs remain stable.
	NOUS_ENGINE_API void RegenerateLibrary();

	NOUS_ENGINE_API bool ResourceExists(uint32 uid) const;
	NOUS_ENGINE_API ResourceBase* CreateResource(const std::string& assetsPath) override;

	// GAME mode variant: load directly from a known library path without reading a .meta file.
	NOUS_ENGINE_API ResourceBase* CreateResourceFromLibrary(uint32 uid, ResourceType type,
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

	NOUS_ENGINE_API bool UnloadResource(uint32 uid) override;

	// Update the in-memory assets path of a loaded resource after it has been moved on disk.
	// No-op if the UID is not currently loaded.
	NOUS_ENGINE_API void UpdateResourcePath(uint32 uid, const std::string& newAssetsPath);

	// Returns a thread-safe snapshot copy of the resources map.
	// Safe to call from any thread (e.g. editor UI) concurrently with resource loading.
	NOUS_ENGINE_API std::unordered_map<uint32, ResourceBase*> GetResourcesMap() const;

	// Takes and clears the pending upload queue — called by Renderer::PreUpdate/Start.
	// Each entry is a resource that has been Deserialized and needs GPU Upload.
	NOUS_ENGINE_API std::vector<std::pair<ResourceType, ResourceBase*>> TakePendingUploads();

	// Takes and clears the pending release queue — called by Renderer::PreUpdate.
	// Each entry is a resource whose ref count hit 0 and needs GPU Release + CPU Evict.
	NOUS_ENGINE_API std::vector<std::pair<ResourceType, ResourceBase*>> TakePendingReleases();

	// Called by Renderer after GPU Release: evicts CPU data and deletes the resource object.
	// Returns true if the resource was evicted, false if it was re-acquired (and re-queued for upload).
	NOUS_ENGINE_API bool EvictResource(ResourceType type, ResourceBase* resource);

	// Wire-compatible with HotReloader::ReadyUpload — kept as a using-alias so
	// ModuleRenderer3D and tests that destructure `[uid, type]` need no change.
	using PendingAssetUpload = HotReloader::ReadyUpload;

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
    [[nodiscard]] NOUS_ENGINE_API ResourceMaterial* GetDefaultMaterial() const override;

    // Returns the resource pointer WITHOUT bumping the reference count (borrowed reference).
    // Use for read-only access (e.g. Inspector UI) where the caller does not own the resource.
    // Do NOT call UnloadResource on the returned pointer.
    // Returns nullptr if the resource is not currently loaded.
    NOUS_ENGINE_API ResourceBase* GetLoadedResource(uint32 uid);

    // Returns the injected importer manager — used by ModuleRenderer3D to call
    // Upload/Release through the IImporterManager interface.
    NOUS_ENGINE_API IImporterManager* GetImporterManager() const;

    // Read-only access to the resource type registry (injected at construction).
    // Used by editor UI windows that hold this module via the editor context.
    NOUS_ENGINE_API const TypeRegistry& GetTypeRegistry() const;

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

	void DeleteResource(ResourceBase*& resource);

	// Spins until the slot's loading thread writes a real pointer, then bumps the refcount and returns it.
	// Returns nullptr if the entry was evicted from the map before it resolved.
	ResourceBase* SpinWaitForSlot(uint32 uid);

	// Instantiates, populates, deserializes, and registers a resource into an already-claimed slot.
	// Caller must have won m_table.TryInsert(uid, nullptr) before calling this.
	ResourceBase* LoadResourceIntoSlot(uint32 uid, ResourceType type,
	    const std::string& name,
	    const std::string& assetsPath,
	    const std::string& libraryPath);

	// Canonical resource registry + the two cross-thread handoff queues.
	ResourceTable           m_table;
	ResourceQueue           m_pendingUploads;
	ResourceQueue           m_pendingReleases;

	// Injected dependencies
	IImporterManager*       mImporterManager = nullptr;
	const TypeRegistry*     mTypeRegistry    = nullptr;
	BuiltinResources        m_builtinResources;
	ImportPipeline          m_importPipeline;
	ScenePreloader          m_scenePreloader;
	HotReloader             m_hotReloader;

	// Composes on top of m_table + m_pendingUploads; must be declared after both.
	SubMeshCache            m_subMeshCache;
};
