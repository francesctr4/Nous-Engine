#pragma once

#include "Module.h"
#include "Resource.h"

using UID = uint32;
struct MetaFileData;

class ModuleResourceManager : public Module
{
public:

	// Constructor
	ModuleResourceManager(Application* app, std::string name, bool start_enabled = true);

	// Destructor
	virtual ~ModuleResourceManager();

	bool Awake() override;
	bool Start() override;

	UpdateStatus PreUpdate(float dt) override;
	UpdateStatus Update(float dt) override;
	UpdateStatus PostUpdate(float dt) override;

	bool CleanUp() override;

	void ReceiveEvent(const Event& event) override;

	// ------------------------------------------------------------------------ //

	bool ImportFile(const std::string& path);

	Resource* CreateResource(const std::string& assetsPath);
	bool UnloadResource(const UID& UID);

	const std::unordered_map<UID, Resource*>& GetResourcesMap() const;

private:

	bool CreateMetaFile(const std::string& metaFilePath, const MetaFileData& inFileData);
	bool ReadMetaFile(const std::string& metaFilePath, MetaFileData& outFileData);

	Resource* InstantiateResource(const ResourceType& type);
	void DeleteResource(Resource*& resource);

	bool ResourceExists(const UID& uid);

	Resource* RequestResource(const UID& uid);

	void AddResource(const UID& uid, Resource*& resource);

	//std::string GetLibraryPath(const std::string& assetsPath);

private:

	std::unordered_map<UID, Resource*> resources;
	std::mutex resourcesMutex;  // Mutex to protect resources map from race conditions
};