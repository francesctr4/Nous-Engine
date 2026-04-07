#ifndef MODULERESOURCEMANAGER_H
#define MODULERESOURCEMANAGER_H

#include "Engine/Modules/Module.h"
#include "Engine/EngineExport.h"
#include "Engine/Core/EventSystem/IEventListener.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"

#include <future>
#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

class IGPUResourceFactory;
class IImporterManager;

using UID = uint32_t;
struct MetaFileData;
class ResourceMesh;

class ResourceTexture;
class ResourceMaterial;

class ModuleResourceManager : public Module, public IEventListener
{
public:

	// Constructor
	NOUS_ENGINE_API ModuleResourceManager(EventSystem* eventSystem, NOUS_Multithreading::NOUS_JobSystem* jobSystem,
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

	NOUS_ENGINE_API bool ResourceExists(const UID& uid) const;
	NOUS_ENGINE_API Resource* CreateResource(const std::string& assetsPath);

	// GAME mode variant: load directly from a known library path without reading a .meta file.
	NOUS_ENGINE_API Resource* CreateResourceFromLibrary(UID uid, ResourceType type,
	                                                    const std::string& name,
	                                                    const std::string& assetsPath,
	                                                    const std::string& libraryPath);

	// GAME mode variant: load a submesh by library path + index, no .meta required.
	// `assetsPath` is optional: when provided (EDITOR path), it is stamped onto the
	// created resource so later serialization (Scene snapshot, Save Scene) can write
	// a non-empty assetPath back out. Without it the resource would carry an empty
	// assetsPath and CMesh::Deserialize would drop the reference on the next load.
	NOUS_ENGINE_API ResourceMesh* RequestOrCreateSubMeshResourceFromLibrary(
	    const std::string& libraryPath, int32_t submeshIndex,
	    const std::string& assetsPath = "");

	NOUS_ENGINE_API bool UnloadResource(const UID& UID);


	// Returns a thread-safe snapshot copy of the resources map.
	// Safe to call from any thread (e.g. editor UI) concurrently with AddResource().
	NOUS_ENGINE_API std::unordered_map<UID, Resource*> GetResourcesMap() const;

	// Takes and clears the pending upload queue — called by Renderer::PreUpdate/Start.
	// Each entry is a resource that has been Deserialized and needs GPU Upload.
	NOUS_ENGINE_API std::vector<std::pair<ResourceType, Resource*>> TakePendingUploads();

	// Takes and clears the pending release queue — called by Renderer::PreUpdate.
	// Each entry is a resource whose ref count hit 0 and needs GPU Release + CPU Evict.
	NOUS_ENGINE_API std::vector<std::pair<ResourceType, Resource*>> TakePendingReleases();

	// Called by Renderer after GPU Release: evicts CPU data and deletes the resource object.
	// Returns true if the resource was evicted, false if it was re-acquired (and re-queued for upload).
	NOUS_ENGINE_API bool EvictResource(ResourceType type, Resource* resource);

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
    NOUS_ENGINE_API Resource* GetLoadedResource(UID uid);

    // Reads the .meta sidecar for assetsPath and fills outData.
    // Returns false if the meta file is missing or malformed.
    static NOUS_ENGINE_API bool GetAssetMetaData(const std::string& assetsPath, MetaFileData& outData) ;

    // Returns the ResourceMesh for a specific submesh within a source asset.
    // If already loaded this session, bumps the ref count and returns it.
    // Otherwise loads the submesh from the library binary, uploads it to the GPU,
    // and registers it in the resource map with a generated UID.
    NOUS_ENGINE_API ResourceMesh* RequestOrCreateSubMeshResource(const std::string& assetsPath,
                                                                  int32_t submeshIndex);

    // Scans a .nous scene file for all CMesh resource requests and submits one parallel
    // Deserialize job per unique (assetPath, submeshIndex) pair.
    // Returns futures — wait on all before calling Scene::Deserialize() so CMesh::Deserialize()
    // hits the resource cache instead of blocking on disk I/O.
    NOUS_ENGINE_API std::vector<std::future<void>> PreloadSceneResourcesAsync(
        NOUS_Multithreading::NOUS_JobSystem* jobSystem,
        const std::string& sceneFilePath);

private:

	static bool EnsureLibraryDirectories();
	static bool CreateMetaFile(const std::string& metaFilePath, const MetaFileData& inFileData);
	static bool ReadMetaFile(const std::string& metaFilePath, MetaFileData& outFileData);

	static Resource* InstantiateResource(const ResourceType& type);
	void DeleteResource(Resource*& resource);

	Resource* RequestResource(const UID& uid);
	void AddResource(const UID& uid, Resource*& resource);

	mutable std::mutex resourcesMutex;  // mutable: const methods (e.g. GetResourcesMap) can lock it
	std::unordered_map<UID, Resource*> resources;

	// Resources waiting for GPU upload — populated by CreateResource/Deserialize paths.
	// Drained by Renderer::PreUpdate (and Start for the initial set).
	std::mutex m_pendingUploadsMutex;
	std::vector<std::pair<ResourceType, Resource*>> m_pendingUploads;

	// Resources whose refcount hit 0 — waiting for GPU Release then CPU Evict.
	// Drained by Renderer::PreUpdate each frame.
	std::mutex m_pendingReleasesMutex;
	std::vector<std::pair<ResourceType, Resource*>> m_pendingReleases;

	// Maps (baseAssetUID, submeshIndex) → sub-resource UID.
	// Allows RequestOrCreateSubMeshResource to reuse already-loaded sub-resources.
	// Entry removed when the sub-resource is destroyed in DeleteResource().
	std::map<std::pair<UID, int32_t>, UID> m_submeshUIDMap;

	ResourceTexture*  mDefaultTexture    = nullptr;
	ResourceTexture*  mWhiteTexture      = nullptr;
	ResourceTexture*  mBlackTexture      = nullptr;
	ResourceTexture*  mFlatNormalTexture = nullptr;
	ResourceMaterial* mDefaultMaterial   = nullptr;

	// Injected dependencies
	IImporterManager* mImporterManager = nullptr;

};

#endif // MODULERESOURCEMANAGER_H