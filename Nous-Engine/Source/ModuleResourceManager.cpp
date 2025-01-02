#include "ModuleResourceManager.h"
#include "Resource.h"

#include "ResourceMesh.h"
#include "ResourceMaterial.h"
#include "ResourceTexture.h"

#include "ModuleInput.h"
#include "FileManager.h"

#include "MemoryManager.h"

#include "Random.h"
#include "JsonFile.h"

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
		Resource* myresourcen = new ResourceMesh(sdggsgr);

		myresourcen->SetName("holadfgrdg");
		myresourcen->SetType(ResourceType::TEXTURE);
		myresourcen->IncreaseReferenceCount();
		myresourcen->SetUID(sdggsgr);
		sdggsgr++;

		resources[myresourcen->GetUID()] = myresourcen;
	}

	if (App->input->GetKey(SDL_SCANCODE_J) == KeyState::DOWN) {
		Resource* myresourcen = new ResourceMesh(sdggsgr);

		myresourcen->SetName("holadfgrdg");
		myresourcen->SetType(ResourceType::MESH);
		myresourcen->IncreaseReferenceCount();
		sdggsgr++;

		resources[myresourcen->GetUID()] = myresourcen;
	}

	if (App->input->GetKey(SDL_SCANCODE_K) == KeyState::DOWN) {
		Resource* myresourcen = new ResourceMesh(sdggsgr);

		myresourcen->SetName("holadfgrdg");
		myresourcen->SetType(ResourceType::MATERIAL);
		myresourcen->IncreaseReferenceCount();
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

		std::string metaFilePath = Resource::GetAssetsDirectoryFromType(resourceType) + fileName + extension + ".meta";

		if (!NOUS_FileManager::Exists(metaFilePath))
		{
			// CASE 1: The file is in "Assets\\" and DOES NOT HAVE Meta File
			// New Resource, Create Meta File

			UID resourceUID = Random::Generate();
			std::string libraryExtension = Resource::GetLibraryExtensionFromType(resourceType);
			std::string libraryPath = Resource::GetLibraryDirectoryFromType(resourceType) +
									  std::to_string(resourceUID) + "." + libraryExtension;

			if (!CreateMetaFile(metaFilePath, fileName, resourceUID, resourceType, relativePath, libraryPath)) 
			{
				NOUS_ERROR("Import File ERROR: CASE 1 --> Error creating meta file: %s", metaFilePath.c_str());
				return false;
			}

			Resource* resource = NOUS_NEW<ResourceMesh>(MemoryManager::MemoryTag::ENTITY, 5);

			resource->SetName(fileName);
			resource->SetUID(resourceUID);
			resource->SetType(resourceType);
			resource->SetAssetsPath(relativePath);
			resource->SetLibraryPath(libraryPath);

			NOUS_DELETE(resource, MemoryManager::MemoryTag::ENTITY);

			/*switch (resourceType)
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
			}*/
		}
		else 
		{
			// CASE 2,3: The file is in "Assets\\" and HAS Meta File
			// Retrieve data from Meta File

			Resource* resource = nullptr;
			InstantiateResource(resource, resourceType);

			if (resource != nullptr) 
			{
				if (!ReadMetaFile(metaFilePath.c_str(), resource))
				{
					NOUS_ERROR("Import File ERROR: CASE 2,3 --> Error reading meta file: %s", metaFilePath.c_str());
					return false;
				}

				if (!NOUS_FileManager::Exists(resource->GetLibraryPath()))
				{
					// CASE 2: The file is in "Assets\\" and HAS Meta File but NO Library File
					// Reimport to create library file with the same UID and data from meta file

				}
				else
				{
					// CASE 3: The file is in "Assets\\" and HAS Meta File AND Library File
					// Load the Library File

					resource->LoadInMemory();

					resources[resource->GetUID()] = resource;
				}
			}
			else 
			{
				NOUS_ERROR("Import File ERROR: CASE 2,3 --> Failed to Instantiate Resource. Returned nullptr.");
				return false;
			}
		}
	}
	else if (fileDirectory.rfind("Library\\", 0) == 0)
	{
		// TODO
		// CASE 4: The file is in "Library\\"

		//Resource* resource = NOUS_NEW<Resource>(MemoryManager::MemoryTag::ENTITY);

		//resource->SetUID(static_cast<UID>(std::stoul(fileName)));
		//resource->SetType(resourceType);
		//resource->SetLibraryPath(path);

		//switch (resourceType)
		//{
		//	case ResourceType::MESH: 
		//	{


		//		break;
		//	}	
		//	case ResourceType::MATERIAL: 
		//	{


		//		break;
		//	}
		//	case ResourceType::TEXTURE: 
		//	{


		//		break;
		//	}
		//	case ResourceType::UNKNOWN:
		//	{
		//		NOUS_ERROR("Import File ERROR: CASE 4 --> Unsupported file extension: %s", extension.c_str());
		//		return false;
		//		break;
		//	}
		//}
	}
	else 
	{
		// DONE
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
			NOUS_ERROR("Import File ERROR: CASE 0 --> Unsupported file extension: %s", extension.c_str());
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

bool ModuleResourceManager::CreateMetaFile(const std::string& metaFilePath, const std::string& name, const UID& uid, 
	const ResourceType& resourceType, const std::string& assetsFilePath, const std::string& libraryFilePath)
{
	JsonFile metaFile;

	metaFile.AppendValue("Name", name);
	metaFile.AppendValue("UID", static_cast<double>(uid));
	metaFile.AppendValue("Resource Type", static_cast<int>(resourceType));
	metaFile.AppendValue("Assets Path", assetsFilePath);
	metaFile.AppendValue("Library Path", libraryFilePath);

	return metaFile.SaveToFile(metaFilePath.c_str());
}

bool ModuleResourceManager::ReadMetaFile(const std::string& metaFilePath, Resource* resource)
{
	JsonFile metaFile;

	if (!metaFile.LoadFromFile(metaFilePath.c_str())) 
	{
		return false;
	}

	std::string fileName, assetsPath, libraryPath;
	double resourceUID;
	int resourceType;

	metaFile.GetValue("Name", fileName);
	metaFile.GetValue("UID", resourceUID);
	metaFile.GetValue("Resource Type", resourceType);
	metaFile.GetValue("Assets Path", assetsPath);
	metaFile.GetValue("Library Path", libraryPath);

	resource->SetName(fileName);
	resource->SetUID(static_cast<UID>(resourceUID));
	resource->SetType(static_cast<ResourceType>(resourceType));
	resource->SetAssetsPath(assetsPath);
	resource->SetLibraryPath(libraryPath);

	return true;
}

void ModuleResourceManager::InstantiateResource(Resource*& resource, const ResourceType& type)
{
	switch (type)
	{
		case ResourceType::MESH:
		{
			resource = NOUS_NEW<ResourceMesh>(MemoryManager::MemoryTag::RESOURCE_MESH);
			break;
		}
		case ResourceType::MATERIAL:
		{
			resource = NOUS_NEW<ResourceMaterial>(MemoryManager::MemoryTag::RESOURCE_MATERIAL);
			break;
		}
		case ResourceType::TEXTURE:
		{
			resource = NOUS_NEW<ResourceTexture>(MemoryManager::MemoryTag::RESOURCE_TEXTURE);
			break;
		}
		case ResourceType::UNKNOWN:
		{
			resource = nullptr;
			break;
		}
	}
}
