#pragma once

#include "Module.h"

#include "Resource.h"
using UID = uint32;

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
	std::unordered_map<UID, Resource*> GetResourcesMap() const;

private:

	std::string GetLibraryDirectory(const std::string& assetsDirectory) const;

	bool CreateMetaFile(const std::string& metaFilePath, const std::string& name, const UID& uid, 
		const ResourceType& resourceType, const std::string& assetsFilePath, const std::string& libraryFilePath);

	bool ReadMetaFile(const std::string& metaFilePath, Resource* resource);

	void InstantiateResource(Resource*& resource, const ResourceType& type);

private:

	std::unordered_map<UID, Resource*> resources;

};