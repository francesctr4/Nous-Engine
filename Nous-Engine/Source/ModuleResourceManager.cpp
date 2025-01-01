#include "ModuleResourceManager.h"
#include "Resource.h"

#include "FileManager.h"

ModuleResourceManager::ModuleResourceManager(Application* app, std::string name, bool start_enabled) : Module(app, name, start_enabled)
{
	NOUS_TRACE("%s()", __FUNCTION__);
}

ModuleResourceManager::~ModuleResourceManager()
{
	NOUS_TRACE("%s()", __FUNCTION__);
}

bool ModuleResourceManager::Awake()
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return true;
}

bool ModuleResourceManager::Start()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	Resource myresourcen;

	UID hola = 278847;

	myresourcen.name = "hola";
	myresourcen.type = ResourceType::MESH;
	myresourcen.referenceCount = 3;
	myresourcen.UID = hola;

	resources[myresourcen.UID] = &myresourcen;

	NOUS_FATAL("%s", resources[hola]->name.c_str());

	return true;
}

UpdateStatus ModuleResourceManager::PreUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return UPDATE_CONTINUE;
}

UpdateStatus ModuleResourceManager::Update(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return UPDATE_CONTINUE;
}

UpdateStatus ModuleResourceManager::PostUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return UPDATE_CONTINUE;
}

bool ModuleResourceManager::CleanUp()
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return true;
}

void ModuleResourceManager::ReceiveEvent(const Event& event)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	switch (event.type)
	{
		case EventType::DROP_FILE:
		{
			ImportFile(event.context.c);

			break;
		}
		default:
		{
			break;
		}
	}
}

void ModuleResourceManager::ImportFile(const std::string& path)
{
	std::string assetsPath = NOUS_FileManager::GetRelativePath(path);
	std::string assetsDirectory = NOUS_FileManager::GetDirectory(path);
	std::string fileName = NOUS_FileManager::GetFilename(path);
	std::string extension = NOUS_FileManager::GetExtension(path);

	std::string libraryDirectory = GetLibraryDirectory(assetsDirectory);
}

std::unordered_map<UID, Resource*> ModuleResourceManager::GetResourcesMap() const
{
	return resources;
}

std::string ModuleResourceManager::GetLibraryDirectory(const std::string& assetsDirectory) const
{
	// Temporary
	return std::string();
}
