#include "ModuleResourceManager.h"
#include "Engine/Systems/Resource Manager/Resource Types/Resource.h"
#include "Engine/Systems/Event System/EventSystem.h"
#include "Engine/Systems/Resource Manager/Resource Types/ResourceMesh.h"
#include "Engine/Systems/Resource Manager/Resource Types/ResourceMaterial.h"
#include "Engine/Systems/Resource Manager/Resource Types/ResourceTexture.h"
#include "Engine/Core/Application.h"
#include "Engine/Core/Modules/ModuleRenderer3D/include/ModuleRenderer3D.h"
#include "Engine/Renderer/Frontend/RendererFrontend.h"
#include "Engine/Core/Modules/ModuleInput/include/ModuleInput.h"
#include "Engine/Systems/File System/FileManager.h"
#include "Engine/Systems/Logging System/Logger.h"
#include "Engine/Systems/Memory Manager/MemoryManager.h"

#include "Engine/Utils/Random.h"
#include "Engine/Utils/JsonFile.h"
#include "Engine/Systems/Resource Manager/MetaFileData.inl"

#include "Engine/Systems/Resource Manager/Importers/ImporterManager.h"

ModuleResourceManager::ModuleResourceManager(Application* app) : Module(app)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	App->eventSystem->Subscribe(EventType::DROP_FILE, this);
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

	// -----------------------
	// Default Texture
	// -----------------------
	NOUS_INFO("Creating default checkerboard texture...");

	const uint32 texDimension = 256;
	const uint32 channels = 4;
	const uint32 pixelCount = texDimension * texDimension;
	const uint32 squareSize = 16;

	std::vector<uint8_t> pixels(pixelCount * channels, 255);
	for (uint32_t row = 0; row < texDimension; ++row)
	{
		for (uint32_t col = 0; col < texDimension; ++col)
		{
			uint32_t index = (row * texDimension) + col;
			uint32_t indexBpp = index * channels;

			bool isWhite = ((row / squareSize) % 2 == (col / squareSize) % 2);

			if (isWhite) {
				pixels[indexBpp + 0] = 255;
				pixels[indexBpp + 1] = 255;
				pixels[indexBpp + 2] = 255;
			} else {
				pixels[indexBpp + 0] = 0;
				pixels[indexBpp + 1] = 0;
				pixels[indexBpp + 2] = 255;
			}
			pixels[indexBpp + 3] = 255;
		}
	}

	mDefaultTexture = NOUS_NEW<ResourceTexture>(MemoryManager::MemoryTag::RESOURCE_TEXTURE);
	mDefaultTexture->SetName("DefaultTexture");
	mDefaultTexture->width = texDimension;
	mDefaultTexture->height = texDimension;
	mDefaultTexture->channelCount = channels;

	if (!App->renderer->GetRendererFrontend()->CreateTexture(pixels.data(), mDefaultTexture))
	{
		NOUS_FATAL("Failed to create default texture.");
		return false;
	}

	// -----------------------
	// Default Material
	// -----------------------
	mDefaultMaterial = NOUS_NEW<ResourceMaterial>(MemoryManager::MemoryTag::RESOURCE_MATERIAL);
	mDefaultMaterial->SetName("DefaultMaterial");
	mDefaultMaterial->diffuseColor = glm::vec4(1.0f);
	mDefaultMaterial->diffuseMap.type = TextureMapType::DIFFUSE;
	mDefaultMaterial->diffuseMap.texture = mDefaultTexture;

	if (!App->renderer->GetRendererFrontend()->CreateMaterial(mDefaultMaterial))
	{
		NOUS_FATAL("Failed to create default material.");
		return false;
	}

	return true;
}

UpdateStatus ModuleResourceManager::PreUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleResourceManager::Update(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);
	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleResourceManager::PostUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return UpdateStatus::CONTINUE;
}

bool ModuleResourceManager::CleanUp()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return true;
}

void ModuleResourceManager::OnEvent(const Event& event)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	switch (event.type)
	{
		case EventType::DROP_FILE:
		{
			ImportFile(event.ctx.c);

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

			UID resourceUID = static_cast<uint32>(Random::Generate());
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
	NOUS_INFO("[%s] Creating Resource", __FUNCTION__);
	NOUS_INFO("Assets path: %s", assetsPath.c_str());

	std::string metaFilePath = assetsPath + ".meta";
	NOUS_INFO("Looking for meta file: %s", metaFilePath.c_str());

	MetaFileData metaFileData;
	if (!ReadMetaFile(metaFilePath, metaFileData))
	{
		NOUS_ERROR("CreateResource ERROR: Failed to read meta file: %s", metaFilePath.c_str());
		return nullptr;
	}

	NOUS_INFO("Meta file read successfully:");
	NOUS_INFO(" - Name: %s", metaFileData.name.c_str());
	NOUS_INFO(" - UID: %u", metaFileData.uid);
	NOUS_INFO(" - Type: %s", Resource::GetLibraryExtensionFromType(metaFileData.resourceType).c_str());
	NOUS_INFO(" - Assets path: %s", metaFileData.assetsPath.c_str());
	NOUS_INFO(" - Library path: %s", metaFileData.libraryPath.c_str());

	if (!ResourceExists(metaFileData.uid))
	{
		NOUS_INFO("Resource with UID %u does not exist. Creating new instance...", metaFileData.uid);

		Resource* resource = InstantiateResource(metaFileData.resourceType);
		if (resource == nullptr)
		{
			NOUS_ERROR("CreateResource ERROR: Failed to instantiate resource of type %s.",
					   Resource::GetLibraryExtensionFromType(metaFileData.resourceType).c_str());
			return nullptr;
		}

		resource->SetName(metaFileData.name);
		resource->SetUID(metaFileData.uid);
		resource->SetType(metaFileData.resourceType);
		resource->SetAssetsPath(metaFileData.assetsPath);
		resource->SetLibraryPath(metaFileData.libraryPath);

		NOUS_INFO("Resource instantiated successfully (UID: %u, Name: %s). Loading from library...",
				  metaFileData.uid, metaFileData.name.c_str());

		if (!ImporterManager::Load(metaFileData.resourceType, metaFileData.libraryPath, resource))
		{
			NOUS_ERROR("CreateResource ERROR: Failed to load resource from library: %s",
					   metaFileData.libraryPath.c_str());
			return nullptr;
		}

		AddResource(metaFileData.uid, resource);

		resource->IncreaseReferenceCount();
		resource->Validate();

		NOUS_INFO("Resource successfully created and loaded into memory.");
		NOUS_INFO("Reference count: %u", resource->GetReferenceCount());
		NOUS_INFO("========================================");

		return resource;
	}
	else
	{
		NOUS_INFO("Resource with UID %u already exists. Requesting existing instance...", metaFileData.uid);
		Resource* existingResource = RequestResource(metaFileData.uid);
		if (existingResource)
		{
			NOUS_INFO("Existing resource retrieved successfully (Name: %s, RefCount: %u).",
					  existingResource->GetName().c_str(), existingResource->GetReferenceCount());

			existingResource->Validate();
		}
		else
		{
			NOUS_ERROR("CreateResource ERROR: Failed to retrieve existing resource with UID %u.", metaFileData.uid);
		}
		NOUS_INFO("========================================");

		return existingResource;
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

    if (mDefaultTexture)
    {
        App->renderer->GetRendererFrontend()->DestroyTexture(mDefaultTexture);
        NOUS_DELETE(mDefaultTexture, MemoryManager::MemoryTag::RESOURCE_TEXTURE);
        mDefaultTexture = nullptr;
    }

    if (mDefaultMaterial)
    {
        App->renderer->GetRendererFrontend()->DestroyMaterial(mDefaultMaterial);
        NOUS_DELETE(mDefaultMaterial, MemoryManager::MemoryTag::RESOURCE_MATERIAL);
        mDefaultMaterial = nullptr;
    }
}

ResourceTexture *ModuleResourceManager::GetDefaultTexture() const
{
    return mDefaultTexture;
}

ResourceMaterial *ModuleResourceManager::GetDefaultMaterial() const
{
    return mDefaultMaterial;
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