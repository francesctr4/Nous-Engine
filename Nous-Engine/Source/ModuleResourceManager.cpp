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

	return true;
}

bool ModuleResourceManager::Start()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return true;
}

UpdateStatus ModuleResourceManager::PreUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	if (App->input->GetKey(SDL_SCANCODE_C) == KeyState::DOWN) 
	{
		ClearResources();
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

	ClearResources();

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
	if (!NOUS_FileManager::Exists(path))
	{
		NOUS_ERROR("Import File ERROR: General --> Couldn't find file: %s", path.c_str());
		return false;
	}

	std::string relativePath = NOUS_FileManager::GetRelativePath(path);
	std::string fileDirectory = NOUS_FileManager::GetDirectory(path);
	std::string fileName = NOUS_FileManager::GetFilename(path);
	std::string extension = NOUS_FileManager::GetExtension(path);

	ResourceType resourceType = Resource::GetTypeFromExtension(extension);

	if (resourceType == ResourceType::UNKNOWN) 
	{
		// NOUS_ERROR("Import File ERROR: General --> Unsupported file extension: %s", extension.c_str());
		return false;
	}

	if (fileDirectory.rfind("Assets\\", 0) == 0)
	{
		// CASE 1,2,3: The file is in "Assets\\"

		std::string metaFilePath = Resource::GetAssetsDirectoryFromType(resourceType) + fileName + extension + ".meta";

		if (!NOUS_FileManager::Exists(metaFilePath))
		{
			// DONE
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

			// Manage inside: Import Resource and Save into Library
			ImporterManager::Import(metaFileData.resourceType, metaFileData);

			// Here we finish importing the file, and we start creating the resource.

			//CreateResource(metaFileData.assetsPath);

			//Resource* resource = InstantiateResource(resourceType);

			//if (resource != nullptr)
			//{
			//	resource->SetName(fileName);
			//	resource->SetUID(resourceUID);
			//	resource->SetType(resourceType);
			//	resource->SetAssetsPath(relativePath);
			//	resource->SetLibraryPath(libraryPath);
			//}
			//else
			//{
			//	NOUS_ERROR("Import File ERROR: CASE 1 --> Failed to Instantiate Resource. Returned nullptr.");
			//	return false;
			//}

			//// Manage inside: Loading in memory & increase reference count. 
			//// Manage inside: Retrieve resource name and assetspath from libraryfile.
			//ImporterManager::Load(metaFileData.resourceType, metaFileData.libraryPath, resource);

			//AddResource(metaFileData.uid, resource);

			//// Push to render packet
			////External->renderer->geometries.push_back(static_cast<ResourceMesh*>(resource));
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

			if (!NOUS_FileManager::Exists(metaFileData.libraryPath))
			{
				// DONE
				// CASE 2: The file is in "Assets\\" and HAS Meta File but NO Library File
				// Reimport to create library file with the same UID and data from meta file

				// Manage inside: Import Resource and Save into Library
				ImporterManager::Import(metaFileData.resourceType, metaFileData);

				// Here we finish importing the file, and we start creating the resource.

				//CreateResource(metaFileData.assetsPath);

				//if (!ResourceExists(metaFileData.uid))
				//{
				//	// Create New Resource Into Scene
				//	Resource* resource = InstantiateResource(resourceType);

				//	if (resource != nullptr)
				//	{
				//		resource->SetName(metaFileData.name);
				//		resource->SetUID(metaFileData.uid);
				//		resource->SetType(metaFileData.resourceType);
				//		resource->SetAssetsPath(metaFileData.assetsPath);
				//		resource->SetLibraryPath(metaFileData.libraryPath);
				//	}
				//	else
				//	{
				//		NOUS_ERROR("Import File ERROR: CASE 2 --> Failed to Instantiate Resource. Returned nullptr.");
				//		return false;
				//	}

				//	// Manage inside: Loading in memory & increase reference count. 
				//	// Manage inside: Retrieve resource name and assetspath from libraryfile.
				//	ImporterManager::Load(metaFileData.resourceType, metaFileData.libraryPath, resource);

				//	AddResource(metaFileData.uid, resource);

				//	// Push to render packet
				//	//External->renderer->geometries.push_back(static_cast<ResourceMesh*>(resource));
				//}
				//else 
				//{
				//	// TODO
				//	// Get Loaded Resource and Increase Reference Count
				//	resources[metaFileData.uid]->IncreaseReferenceCount();
				//}
			}
			else
			{
				// DONE
				// CASE 3: The file is in "Assets\\" and HAS Meta File AND Library File
				// Load the Library File

				// Here we finish importing the file, and we start creating the resource.

				//CreateResource(metaFileData.assetsPath);

				//if (!ResourceExists(metaFileData.uid))
				//{
				//	// Create New Resource Into Scene
				//	Resource* resource = InstantiateResource(resourceType);

				//	if (resource != nullptr)
				//	{
				//		resource->SetName(metaFileData.name);
				//		resource->SetUID(metaFileData.uid);
				//		resource->SetType(metaFileData.resourceType);
				//		resource->SetAssetsPath(metaFileData.assetsPath);
				//		resource->SetLibraryPath(metaFileData.libraryPath);
				//	}
				//	else
				//	{
				//		NOUS_ERROR("Import File ERROR: CASE 3 --> Failed to Instantiate Resource. Returned nullptr.");
				//		return false;
				//	}

				//	// Manage inside: Loading in memory & increase reference count. 
				//	// Manage inside: Retrieve resource name and assetspath from libraryfile.
				//	ImporterManager::Load(metaFileData.resourceType, metaFileData.libraryPath, resource);

				//	AddResource(metaFileData.uid, resource);

				//	// Push to render packet
				//	//External->renderer->geometries.push_back(static_cast<ResourceMesh*>(resource));
				//}
				//else
				//{
				//	// TODO
				//	// Get Loaded Resource and Increase Reference Count
				//	resources[metaFileData.uid]->IncreaseReferenceCount();
				//}
			}
		}
	}
	else if (fileDirectory.rfind("Library\\", 0) == 0)
	{
		// DONE
		// CASE 4: The file is in "Library\\"
		// Load the Library File

		// Here we finish importing the file, and we start creating the resource.

		//UID resourceUID = static_cast<UID>(std::stoul(fileName));

		//if (!ResourceExists(resourceUID))
		//{
		//	// Create New Resource Into Scene
		//	Resource* resource = InstantiateResource(resourceType);

		//	if (resource != nullptr)
		//	{
		//		//resource->SetName(metaFileData.name);
		//		resource->SetUID(resourceUID);
		//		resource->SetType(resourceType);
		//		//resource->SetAssetsPath(metaFileData.assetsPath);
		//		resource->SetLibraryPath(path);
		//	}
		//	else
		//	{
		//		NOUS_ERROR("Import File ERROR: CASE 4 --> Failed to Instantiate Resource. Returned nullptr.");
		//		return false;
		//	}

		//	// Manage inside: Loading in memory & increase reference count. 
		//	// Manage inside: Retrieve resource name and assetspath from libraryfile.
		//	ImporterManager::Load(resource->GetType(), resource->GetLibraryPath(), resource);

		//	AddResource(resource->GetUID(), resource);

		//	// Push to render packet
		//	//External->renderer->geometries.push_back(static_cast<ResourceMesh*>(resource));
		//}
		//else
		//{
		//	// TODO
		//	// Get Loaded Resource and Increase Reference Count
		//	resources[resourceUID]->IncreaseReferenceCount();
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

const std::unordered_map<UID, Resource*>& ModuleResourceManager::GetResourcesMap() const
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
	UID uid = resource->GetUID();

	switch (resource->GetType())
	{
		case ResourceType::MESH:
		{
			ResourceMesh* r = down_cast<ResourceMesh*>(resource);
			NOUS_DELETE<ResourceMesh>(r, MemoryManager::MemoryTag::RESOURCE_MESH);
			break;
		}
		case ResourceType::MATERIAL:
		{
			ResourceMaterial* r = down_cast<ResourceMaterial*>(resource);
			NOUS_DELETE<ResourceMaterial>(r, MemoryManager::MemoryTag::RESOURCE_MATERIAL);
			break;
		}
		case ResourceType::TEXTURE:
		{
			ResourceTexture* r = down_cast<ResourceTexture*>(resource);
			NOUS_DELETE<ResourceTexture>(r, MemoryManager::MemoryTag::RESOURCE_TEXTURE);
			break;
		}
	}

	resources.erase(uid);
}

bool ModuleResourceManager::ResourceExists(const UID& uid)
{
	return !(resources.find(uid) == resources.end());
}

Resource* ModuleResourceManager::CreateResource(const std::string& assetsPath)
{
	// TODO: Needs to be reworked
	std::string metaFilePath = assetsPath + ".meta";

	MetaFileData metaFileData;
	if (!ReadMetaFile(metaFilePath, metaFileData))
	{
		NOUS_ERROR("Create Resource ERROR: Error reading meta file: %s", metaFilePath.c_str());
		return nullptr;
	}

	if (!ResourceExists(metaFileData.uid))
	{
		// Create New Resource Into Scene
		Resource* resource = InstantiateResource(metaFileData.resourceType);

		if (resource != nullptr)
		{
			resource->SetName(metaFileData.name);
			resource->SetUID(metaFileData.uid);
			resource->SetType(metaFileData.resourceType);
			resource->SetAssetsPath(metaFileData.assetsPath);
			resource->SetLibraryPath(metaFileData.libraryPath);
		}
		else
		{
			NOUS_ERROR("Create Resource ERROR: CASE New Resource --> Failed to Instantiate Resource. Returned nullptr.");
			return nullptr;
		}

		// Manage inside: Loading in memory & increase reference count. 
		// Manage inside: Retrieve resource name and assetspath from libraryfile.
		if (!ImporterManager::Load(metaFileData.resourceType, metaFileData.libraryPath, resource))
		{
			NOUS_ERROR("Create Resource ERROR: CASE New Resource --> Failed to Load Resource From Library. Returned nullptr.");
			return nullptr;
		}

		AddResource(metaFileData.uid, resource);

		resource->IncreaseReferenceCount();

		return resource;
	}
	else
	{
		return RequestResource(metaFileData.uid);
	}
}

bool ModuleResourceManager::UnloadResource(const UID& UID)
{
	if (!ResourceExists(UID))
	{
		return false;
	}

	Resource* tmpResource = resources[UID];

	ImporterManager::Unload(tmpResource->GetType(), tmpResource);

	tmpResource->DecreaseReferenceCount();

	if (tmpResource->GetReferenceCount() == 0)
	{
		DeleteResource(tmpResource);
	}
	
	return true;
}

Resource* ModuleResourceManager::RequestResource(const UID& uid)
{
	Resource* resource = resources[uid];

	ImporterManager::Load(resource->GetType(), resource->GetLibraryPath(), resource);

	resource->IncreaseReferenceCount();

	return resource;
}

void ModuleResourceManager::AddResource(const UID& uid, Resource*& resource)
{
	std::lock_guard<std::mutex> lock(resourcesMutex);  // Multi-threading
	resources[uid] = resource;
}

void ModuleResourceManager::ClearResources()
{
	for (auto& [UID, Resource] : resources)
	{
		ImporterManager::Unload(Resource->GetType(), Resource);

		switch (Resource->GetType())
		{
			case ResourceType::MESH:
			{
				ResourceMesh* r = down_cast<ResourceMesh*>(Resource);
				NOUS_DELETE<ResourceMesh>(r, MemoryManager::MemoryTag::RESOURCE_MESH);
				break;
			}
			case ResourceType::MATERIAL:
			{
				ResourceMaterial* r = down_cast<ResourceMaterial*>(Resource);
				NOUS_DELETE<ResourceMaterial>(r, MemoryManager::MemoryTag::RESOURCE_MATERIAL);
				break;
			}
			case ResourceType::TEXTURE:
			{
				ResourceTexture* r = down_cast<ResourceTexture*>(Resource);
				NOUS_DELETE<ResourceTexture>(r, MemoryManager::MemoryTag::RESOURCE_TEXTURE);
				break;
			}
		}
	}

	resources.clear();
}

//std::string ModuleResourceManager::GetLibraryPath(const std::string& assetsPath)
//{
//	JsonFile metaFile;
//
//	// Load the JSON file
//	if (!metaFile.LoadFromFile((assetsPath + ".meta").c_str()))
//	{
//		return "";
//	}
//
//	std::string libraryPath;
//
//	// Retrieve the value
//	if (!metaFile.GetValue("Library Path", libraryPath))
//	{
//		return "";
//	}
//
//	return libraryPath;
//}