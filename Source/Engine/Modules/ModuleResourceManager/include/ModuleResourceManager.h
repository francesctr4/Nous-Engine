#ifndef MODULERESOURCEMANAGER_H
#define MODULERESOURCEMANAGER_H

#include "Engine/Modules/Module.h"
#include "Engine/EngineExport.h"
#include "Engine/Core/EventSystem/IEventListener.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"

#include <mutex>
#include <unordered_map>

using UID = uint32_t;
struct MetaFileData;

class ResourceTexture;
class ResourceMaterial;

class ModuleResourceManager : public Module, public IEventListener
{
public:

	// Constructor
	ModuleResourceManager(Application* app);

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
	NOUS_ENGINE_API bool UnloadResource(const UID& UID);

	// Returns a thread-safe snapshot copy of the resources map.
	// Safe to call from any thread (e.g. editor UI) concurrently with AddResource().
	NOUS_ENGINE_API std::unordered_map<UID, Resource*> GetResourcesMap() const;

	NOUS_ENGINE_API void ClearResources();

    [[nodiscard]] ResourceTexture* GetDefaultTexture() const;
    [[nodiscard]] ResourceMaterial* GetDefaultMaterial() const;

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

	ResourceTexture* mDefaultTexture = nullptr;
	ResourceMaterial* mDefaultMaterial = nullptr;
};

#endif // MODULERESOURCEMANAGER_H