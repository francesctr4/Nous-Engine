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
#include "MetaFileData.inl"

#include "ImporterManager.h"

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

	ImporterManager::Initialize();

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

	ImporterManager::Shutdown();

	for (auto& [UID, Resource] : resources)
	{
		DeleteResource(Resource);
	}

	resources.clear();

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

			MetaFileData metaFileData;

			metaFileData.name = fileName;
			metaFileData.uid = resourceUID;
			metaFileData.resourceType = resourceType;
			metaFileData.assetsPath = relativePath;
			metaFileData.libraryPath = libraryPath;

			if (!CreateMetaFile(metaFilePath, metaFileData)) 
			{
				NOUS_ERROR("Import File ERROR: CASE 1 --> Error creating meta file: %s", metaFilePath.c_str());
				return false;
			}

			//Resource* resource = NOUS_NEW<ResourceMesh>(MemoryManager::MemoryTag::ENTITY, 5);

			//resource->SetName(fileName);
			//resource->SetUID(resourceUID);
			//resource->SetType(resourceType);
			//resource->SetAssetsPath(relativePath);
			//resource->SetLibraryPath(libraryPath);

			//NOUS_DELETE(resource, MemoryManager::MemoryTag::ENTITY);
		}
		else 
		{
			// CASE 2,3: The file is in "Assets\\" and HAS Meta File
			// Retrieve data from Meta File

			MetaFileData metaFileData;

			if (!ReadMetaFile(metaFilePath, metaFileData))
			{
				NOUS_ERROR("Import File ERROR: CASE 2,3 --> Error reading meta file: %s", metaFilePath.c_str());
				return false;
			}

			if (!ResourceExists(metaFileData.uid))
			{
				// Create a new resource
				Resource* resource = InstantiateResource(metaFileData.resourceType);

				if (resource != nullptr)
				{
					resource->SetName(metaFileData.name);
					resource->SetUID(metaFileData.uid);
					resource->SetType(metaFileData.resourceType);
					resource->SetAssetsPath(metaFileData.assetsPath);
					resource->SetLibraryPath(metaFileData.libraryPath);

					if (!NOUS_FileManager::Exists(resource->GetLibraryPath()))
					{
						// CASE 2: The file is in "Assets\\" and HAS Meta File but NO Library File
						// Reimport to create library file with the same UID and data from meta file
						ImporterManager::Import(metaFileData.resourceType, metaFileData.assetsPath);

						resource->LoadInMemory();
						resource->IncreaseReferenceCount();

						resources[metaFileData.uid] = resource;
					}
					else
					{
						// DONE
						// CASE 3: The file is in "Assets\\" and HAS Meta File AND Library File
						// Load the Library File

						resource->LoadInMemory();
						resource->IncreaseReferenceCount();

						resources[resource->GetUID()] = resource;
					}
				}
				else
				{
					NOUS_ERROR("Import File ERROR: CASE 2,3 --> Failed to Instantiate Resource. Returned nullptr.");
					return false;
				}
			}
			else 
			{
				// Get existing resource
				NOUS_WARN("Import File WARNING: CASE 2,3 --> Attempted to import an existing resource.");
				resources[metaFileData.uid]->IncreaseReferenceCount();
			}
		}
	}
	else if (fileDirectory.rfind("Library\\", 0) == 0)
	{
		// DONE
		// CASE 4: The file is in "Library\\"
		// Load the Library File

		UID resourceUID = static_cast<UID>(std::stoul(fileName));

		if (!ResourceExists(resourceUID)) 
		{
			// Create a new resource
			Resource* resource = InstantiateResource(resourceType);

			if (resource != nullptr)
			{
				resource->SetUID(resourceUID);
				resource->SetType(resourceType);
				resource->SetLibraryPath(path);

				resource->LoadInMemory(); // Retrieve name and assetspath inside, from libraryFile
				//resource->SetName(fileName);
				//resource->SetAssetsPath(path);

				resource->IncreaseReferenceCount();

				resources[resource->GetUID()] = resource;
			}
			else
			{
				NOUS_ERROR("Import File ERROR: CASE 4 --> Failed to Instantiate Resource. Returned nullptr.");
				return false;
			}
		}
		else 
		{
			// Get existing resource
			NOUS_WARN("Import File WARNING: CASE 4 --> Attempted to import an existing resource.");
			resources[resourceUID]->IncreaseReferenceCount();
		}
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

bool ModuleResourceManager::CreateMetaFile(const std::string& metaFilePath, const MetaFileData& inFileData)
{
	JsonFile metaFile;

	metaFile.AppendValue("Name", inFileData.name);
	metaFile.AppendValue("UID", static_cast<double>(inFileData.uid));
	metaFile.AppendValue("Resource Type", static_cast<int>(inFileData.resourceType));
	metaFile.AppendValue("Assets Path", inFileData.assetsPath);
	metaFile.AppendValue("Library Path", inFileData.libraryPath);

	return metaFile.SaveToFile(metaFilePath.c_str());
}

bool ModuleResourceManager::ReadMetaFile(const std::string& metaFilePath, MetaFileData& outFileData)
{
	JsonFile metaFile;

	// Load the JSON file
	if (!metaFile.LoadFromFile(metaFilePath.c_str()))
	{
		return false;
	}

	// Variables to hold the intermediate values
	std::string r_fileName, r_assetsPath, r_libraryPath;
	int r_resourceType;
	double r_resourceUID;

	// Retrieve values using GetValue
	if (!metaFile.GetValue("Name", r_fileName) ||
		!metaFile.GetValue("UID", r_resourceUID) ||
		!metaFile.GetValue("Resource Type", r_resourceType) ||
		!metaFile.GetValue("Assets Path", r_assetsPath) ||
		!metaFile.GetValue("Library Path", r_libraryPath))
	{
		return false; // Return false if any required field is missing or invalid
	}

	// Assign values to outFileData, casting where necessary
	outFileData.name = r_fileName;
	outFileData.uid = static_cast<UID>(r_resourceUID); // Casting double to UID
	outFileData.resourceType = static_cast<ResourceType>(r_resourceType); // Casting int to ResourceType
	outFileData.assetsPath = r_assetsPath;
	outFileData.libraryPath = r_libraryPath;

	return true;
}

Resource* ModuleResourceManager::InstantiateResource(const ResourceType& type)
{
	Resource* resource = nullptr;

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
	}

	return resource;
}

void ModuleResourceManager::DeleteResource(Resource*& resource)
{
	switch (resource->GetType())
	{
		case ResourceType::MESH:
		{
			NOUS_DELETE(resource, MemoryManager::MemoryTag::RESOURCE_MESH);
			break;
		}
		case ResourceType::MATERIAL:
		{
			NOUS_DELETE(resource, MemoryManager::MemoryTag::RESOURCE_MATERIAL);
			break;
		}
		case ResourceType::TEXTURE:
		{
			NOUS_DELETE(resource, MemoryManager::MemoryTag::RESOURCE_TEXTURE);
			break;
		}
	}
}

bool ModuleResourceManager::ResourceExists(const UID& uid)
{
	return !(resources.find(uid) == resources.end());
}

Resource* ModuleResourceManager::RequestResource(const UID& uid)
{
	if (!ResourceExists(uid)) 
	{
		return nullptr;
	}

	resources[uid]->LoadInMemory();
	resources[uid]->IncreaseReferenceCount();

	return resources[uid];
}