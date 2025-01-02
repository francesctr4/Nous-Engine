#include "ModuleResourceManager.h"
#include "Resource.h"

#include "ModuleInput.h"
#include "FileManager.h"

#include "MemoryManager.h"

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
	return true;
}

static int sdggsgr = 0;

UpdateStatus ModuleResourceManager::PreUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	if (App->input->GetKey(SDL_SCANCODE_H) == KeyState::DOWN) {
		Resource* myresourcen = new Resource();

		myresourcen->SetName("holadfgrdg");
		myresourcen->SetType(ResourceType::TEXTURE);
		myresourcen->IncreaseReferenceCount();
		myresourcen->SetUID(sdggsgr);
		sdggsgr++;

		resources[myresourcen->GetUID()] = myresourcen;
	}

	if (App->input->GetKey(SDL_SCANCODE_J) == KeyState::DOWN) {
		Resource* myresourcen = new Resource();

		myresourcen->SetName("holadfgrdg");
		myresourcen->SetType(ResourceType::MESH);
		myresourcen->IncreaseReferenceCount();
		myresourcen->SetUID(sdggsgr);
		sdggsgr++;

		resources[myresourcen->GetUID()] = myresourcen;
	}

	if (App->input->GetKey(SDL_SCANCODE_K) == KeyState::DOWN) {
		Resource* myresourcen = new Resource();

		myresourcen->SetName("holadfgrdg");
		myresourcen->SetType(ResourceType::MATERIAL);
		myresourcen->IncreaseReferenceCount();
		myresourcen->SetUID(sdggsgr);
		sdggsgr++;

		resources[myresourcen->GetUID()] = myresourcen;
	}

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

bool ModuleResourceManager::ImportFile(const std::string& path)
{
	std::string relativePath = NOUS_FileManager::GetRelativePath(path);
	std::string fileDirectory = NOUS_FileManager::GetDirectory(path);
	std::string fileName = NOUS_FileManager::GetFilename(path);
	std::string extension = NOUS_FileManager::GetExtension(path);

	ResourceType resourceType = Resource::GetTypeFromExtension(extension);

	if (fileDirectory.rfind("Assets\\", 0) == 0)
	{
		// CASE 1,2,3: The file is in "Assets\\"
		
	}
	else if (fileDirectory.rfind("Library\\", 0) == 0)
	{
		// CASE 4: The file is in "Library\\"

		//Resource* resource = NOUS_NEW<Resource>(MemoryManager::MemoryTag::ENTITY);

		//resource->SetUID(static_cast<UID>(std::stoul(fileName)));
		//resource->SetType(resourceType);
		//resource->SetLibraryPath(path);

		switch (resourceType)
		{
			case ResourceType::MESH: 
			{


				break;
			}	
			case ResourceType::MATERIAL: 
			{
				break;
			}
			case ResourceType::TEXTURE: 
			{
				break;
			}
			case ResourceType::UNKNOWN:
			{
				break;
			}
		}
	}
	else 
	{
		// CASE 0: The file is not in "Assets\\" nor "Library\\"
		// Copy to "Assets\\"

		if (resourceType != ResourceType::UNKNOWN)
		{
			std::string newPath = Resource::GetAssetsDirectoryFromType(resourceType) + fileName + extension;

			if (NOUS_FileManager::CopyFile(path, newPath))
			{
				ImportFile(newPath);
			}
			else
			{
				NOUS_ERROR("Import File ERROR: CASE 0 --> Error while copying the file to Assets\\ directory.");
				return false;
			}
		}
		else 
		{
			NOUS_ERROR("Import File ERROR: CASE 0 --> Unknown file extension: %s", extension.c_str());
			return false;
		}
	}

	return true;
}

std::unordered_map<UID, Resource*> ModuleResourceManager::GetResourcesMap() const
{
	return resources;
}

std::string ModuleResourceManager::GetLibraryDirectory(const std::string& assetsDirectory) const
{
	// TODO
	return std::string();
}