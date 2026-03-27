#ifndef MODULERESOURCEMANAGER_H
#define MODULERESOURCEMANAGER_H

#include "Engine/Modules/Module.h"
#include "Engine/EngineExport.h"
#include "Engine/Core/EventSystem/IEventListener.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"

class IGPUResourceFactory;

#include <map>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using UID = uint32_t;
struct MetaFileData;
class ResourceMesh;

class ResourceTexture;
class ResourceMaterial;

class ModuleResourceManager : public Module, public IEventListener
{
public:

	// Constructor
	ModuleResourceManager(EventSystem* eventSystem, NOUS_Multithreading::NOUS_JobSystem* jobSystem, bool isGameMode);

	// Destructor
	virtual ~ModuleResourceManager();

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

	NOUS_ENGINE_API bool ResourceExists(const UID& uid);
	NOUS_ENGINE_API Resource* CreateResource(const std::string& assetsPath);

	// GAME mode variant: load directly from a known library path without reading a .meta file.
	NOUS_ENGINE_API Resource* CreateResourceFromLibrary(UID uid, ResourceType type,
	                                                    const std::string& name,
	                                                    const std::string& assetsPath,
	                                                    const std::string& libraryPath);

	// GAME mode variant: load a submesh by library path + index, no .meta required.
	NOUS_ENGINE_API ResourceMesh* RequestOrCreateSubMeshResourceFromLibrary(
	    const std::string& libraryPath, int32_t submeshIndex);

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
	NOUS_ENGINE_API void EvictResource(ResourceType type, Resource* resource);

	// Synchronous full teardown — caller must pass the GPU factory so GPU handles can
	// be freed before Vulkan is shut down.  Only called from ModuleRenderer3D::CleanUp().
	NOUS_ENGINE_API void ClearResources(IGPUResourceFactory* gpu);

    [[nodiscard]] ResourceTexture* GetDefaultTexture() const;
    [[nodiscard]] ResourceMaterial* GetDefaultMaterial() const;

    // Reads the .meta sidecar for assetsPath and fills outData.
    // Returns false if the meta file is missing or malformed.
    NOUS_ENGINE_API bool GetAssetMetaData(const std::string& assetsPath, MetaFileData& outData);

    // Returns the ResourceMesh for a specific submesh within a source asset.
    // If already loaded this session, bumps the ref count and returns it.
    // Otherwise loads the submesh from the library binary, uploads it to the GPU,
    // and registers it in the resource map with a generated UID.
    NOUS_ENGINE_API ResourceMesh* RequestOrCreateSubMeshResource(const std::string& assetsPath,
                                                                  int32_t submeshIndex);

private:

	bool EnsureLibraryDirectories();

	bool CreateMetaFile(const std::string& metaFilePath, const MetaFileData& inFileData);
	bool ReadMetaFile(const std::string& metaFilePath, MetaFileData& outFileData);


	Resource* InstantiateResource(const ResourceType& type);
	void DeleteResource(Resource*& resource);

	Resource* RequestResource(const UID& uid);
	void AddResource(const UID& uid, Resource*& resource);

	//std::string GetLibraryPath(const std::string& assetsPath);

private:

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

	bool m_isGameMode;

	ResourceTexture* mDefaultTexture = nullptr;
	ResourceMaterial* mDefaultMaterial = nullptr;

};

#endif // MODULERESOURCEMANAGER_H