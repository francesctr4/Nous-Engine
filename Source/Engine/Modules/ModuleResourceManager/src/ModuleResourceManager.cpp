#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Renderer/IGPUResourceFactory.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include "Engine/Core/FileSystem/FileSystem.h"
#include <filesystem>
#include <algorithm>
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Utils/Serialization/Random/Random.h"
#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"
#include "Engine/Systems/ResourceManager/Resource/MetaFileData.inl"

#include "Engine/Systems/ResourceManager/Importer/ImporterManager.h"
#include "Engine/Systems/ResourceManager/Importer/ImporterMesh/include/ImporterMesh.h"
#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_RESOURCEMANAGER;

ModuleResourceManager::ModuleResourceManager(EventSystem* eventSystem, NOUS_Multithreading::NOUS_JobSystem* jobSystem, bool isGameMode)
    : Module(eventSystem, jobSystem), m_isGameMode(isGameMode)
{
	eventSystem->Subscribe(EventType::DROP_FILE, this);
}

void ModuleResourceManager::SetGPUFactory(IGPUResourceFactory* gpuFactory)
{
	mGPUFactory = gpuFactory;
}

ModuleResourceManager::~ModuleResourceManager()
{

}

bool ModuleResourceManager::Awake()
{
	ImporterManager::Init(this, mGPUFactory);

	if (m_isGameMode)
	{
		// GAME mode: Library binaries are pre-built. No asset scanning/importing needed.
		return true;
	}

	// Always ensure directories exist — idempotent, safe to call every startup.
	EnsureLibraryDirectories();

	// EDITOR mode: scan Assets/ on startup. ImportFile is a cheap no-op for assets
	// whose library binary is already up-to-date (Case 3 timestamp check).
	ImportDirectory("Assets");

	// Mirror all scene files from Assets/Scenes/ → Library/Scenes/ so GameApp
	// can always load them from Library/ without needing Assets/.
	if (NOUS_FileManager::Exists("Assets/Scenes"))
	{
		for (const auto& entry : std::filesystem::directory_iterator("Assets/Scenes"))
		{
			if (entry.path().extension() == ".nous")
			{
				const std::string src  = entry.path().string();
				const std::string dest = "Library/Scenes/" + entry.path().filename().string();
				NOUS_FileManager::CopyFile(src, dest);
			}
		}
	}

	return true;
}

bool ModuleResourceManager::EnsureLibraryDirectories()
{
	return NOUS_FileManager::CreateDirectory("Library") &&
		   NOUS_FileManager::CreateDirectory("Library/Shaders") &&
		   NOUS_FileManager::CreateDirectory("Library/Meshes") &&
		   NOUS_FileManager::CreateDirectory("Library/Materials") &&
		   NOUS_FileManager::CreateDirectory("Library/Textures") &&
		   NOUS_FileManager::CreateDirectory("Library/Scenes");
}

bool ModuleResourceManager::ImportDirectory(const std::string& directory)
{
	if (!NOUS_FileManager::Exists(directory))
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "Directory does not exist: %s", directory.c_str());
		return false;
	}

	for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
	{
		if (std::filesystem::is_regular_file(entry))
		{
			ImportFile(entry.path().string());
		}
	}

	return true;
}

bool ModuleResourceManager::Start()
{
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

	mDefaultTexture = NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE);
	mDefaultTexture->SetName("DefaultTexture");
	mDefaultTexture->width = texDimension;
	mDefaultTexture->height = texDimension;
	mDefaultTexture->channelCount = channels;

	if (!mGPUFactory->CreateTexture(pixels.data(), mDefaultTexture))
	{
		NOUS_FATAL("Failed to create default texture.");
		return false;
	}

	// Give the default texture a stable, valid ID and generation so the
	// descriptor lazy-write cache (WriteInstanceSampler) can distinguish it
	// from the "never written" sentinel (UINT32_MAX). Without this, every draw
	// using the default texture re-fires vkUpdateDescriptorSets, which causes
	// validation errors when multiple objects share the same material instance.
	mDefaultTexture->ID         = 0;
	mDefaultTexture->generation = 0;

	// -----------------------
	// Default Material
	// -----------------------
	mDefaultMaterial = NOUS_NEW<ResourceMaterial>(MemoryTag::RESOURCE_MATERIAL);
	mDefaultMaterial->SetName("DefaultMaterial");
	mDefaultMaterial->diffuseColor = glm::vec4(1.0f);
	mDefaultMaterial->diffuseMap.type = TextureMapType::DIFFUSE;
	mDefaultMaterial->diffuseMap.texture = mDefaultTexture;

	if (!mGPUFactory->CreateMaterial(mDefaultMaterial))
	{
		NOUS_FATAL("Failed to create default material.");
		return false;
	}

	return true;
}

UpdateStatus ModuleResourceManager::PreUpdate(float dt)
{
	// Flush pending unloads — deferred from UnloadResource to avoid destroying
	// GPU resources mid-frame (between command recording and vkQueueSubmit).
	// By the time PreUpdate runs the previous frame has been fully submitted.
	std::vector<UID> toDestroy;
	{
		std::lock_guard<std::mutex> lock(m_PendingUnloadsMutex);
		std::swap(toDestroy, m_PendingUnloads);
	}

	for (const UID& uid : toDestroy)
	{
		if (!ResourceExists(uid)) continue;
		Resource* resource = resources[uid];
		if (!resource) continue; // still loading (placeholder); skip
		if (resource->GetReferenceCount() > 0) continue; // re-acquired since queuing; skip
		ImporterManager::Unload(resource->GetType(), resource);
		DeleteResource(resource);
	}

	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleResourceManager::Update(float dt)
{
	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleResourceManager::PostUpdate(float dt)
{
	return UpdateStatus::CONTINUE;
}

bool ModuleResourceManager::CleanUp()
{
	return true;
}

void ModuleResourceManager::OnEvent(const Event& event)
{
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

// Returns the effective modification time of a library output path.
// Shader library "paths" are directories containing .spv files — Windows does
// not reliably update a directory's own mtime when its contents change, so we
// scan for the newest regular file inside it instead.
static std::filesystem::file_time_type GetLibraryTime(const std::filesystem::path& libraryPath)
{
	namespace fs = std::filesystem;

	if (fs::is_directory(libraryPath))
	{
		auto newest = fs::file_time_type::min();
		for (const auto& entry : fs::directory_iterator(libraryPath))
			if (fs::is_regular_file(entry))
				newest = std::max(newest, fs::last_write_time(entry));
		return newest;
	}

	return fs::last_write_time(libraryPath);
}

bool ModuleResourceManager::ImportFile(const std::string& path)
{
	NOUS_DEBUG_C(CURRENT_CHANNEL, "Importing file: %s", path.c_str());

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
									  std::to_string(resourceUID);
			if (!libraryExtension.empty())
				libraryPath += "." + libraryExtension;

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
			//
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
				//
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
				// CASE 3: The file is in "Assets\\" and HAS Meta File AND Library File.
				// Compare filesystem timestamps: if the source asset is newer than the
				// library binary, the binary is stale and must be regenerated.

				namespace fs = std::filesystem;

				const fs::file_time_type assetTime   = fs::last_write_time(metaFileData.assetsPath);
				const fs::file_time_type libraryTime = GetLibraryTime(metaFileData.libraryPath);

				if (assetTime > libraryTime)
				{
					NOUS_INFO_C(CURRENT_CHANNEL,
						"[ImportFile] '%s' modified since last import — regenerating library binary.",
						metaFileData.name.c_str());

					ImporterManager::Import(metaFileData.resourceType, metaFileData);
				}
				// else: library is up to date — nothing to do.
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
		//
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

std::unordered_map<UID, Resource*> ModuleResourceManager::GetResourcesMap() const
{
	std::lock_guard<std::mutex> lock(resourcesMutex);
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
			resource = NOUS_NEW<ResourceMesh>(MemoryTag::RESOURCE_MESH);
			break;
		}
		case ResourceType::MATERIAL:
		{
			resource = NOUS_NEW<ResourceMaterial>(MemoryTag::RESOURCE_MATERIAL);
			break;
		}
		case ResourceType::TEXTURE:
		{
			resource = NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE);
			break;
		}
		case ResourceType::SHADER:
		{
			resource = NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER);
			break;
		}
		default: break;
	}

	return resource;
}

void ModuleResourceManager::DeleteResource(Resource*& resource)
{
	UID uid = resource->GetUID();

	// Remove any sub-resource map entry that points to this UID.
	for (auto it = m_submeshUIDMap.begin(); it != m_submeshUIDMap.end(); )
	{
		if (it->second == uid)
			it = m_submeshUIDMap.erase(it);
		else
			++it;
	}

	switch (resource->GetType())
	{
		case ResourceType::MESH:
		{
			NOUS_DELETE(resource, MemoryTag::RESOURCE_MESH);
			break;
		}
		case ResourceType::MATERIAL:
		{
			NOUS_DELETE(resource, MemoryTag::RESOURCE_MATERIAL);
			break;
		}
		case ResourceType::TEXTURE:
		{
			NOUS_DELETE(resource, MemoryTag::RESOURCE_TEXTURE);
			break;
		}
		case ResourceType::SHADER:
		{
			NOUS_DELETE(resource, MemoryTag::RESOURCE_SHADER);
			break;
		}
		default: break;
	}

	resources.erase(uid);
}

bool ModuleResourceManager::ResourceExists(const UID& uid)
{
	return !(resources.find(uid) == resources.end());
}

Resource* ModuleResourceManager::CreateResource(const std::string& assetsPath)
{
	NOUS_INFO("Creating Resource");
	NOUS_INFO("Assets path: %s", assetsPath.c_str());

	std::string metaFilePath = assetsPath + ".meta";
	NOUS_INFO("Looking for meta file: %s", metaFilePath.c_str());

	MetaFileData metaFileData;
	if (!ReadMetaFile(metaFilePath, metaFileData))
	{
		NOUS_ERROR("Failed to read meta file: %s", metaFilePath.c_str());
		return nullptr;
	}

	NOUS_INFO("Meta file read successfully:");
	NOUS_INFO(" - Name: %s", metaFileData.name.c_str());
	NOUS_INFO(" - UID: %u", metaFileData.uid);
	NOUS_INFO(" - Type: %s", Resource::GetLibraryExtensionFromType(metaFileData.resourceType).c_str());
	NOUS_INFO(" - Assets path: %s", metaFileData.assetsPath.c_str());
	NOUS_INFO(" - Library path: %s", metaFileData.libraryPath.c_str());

	// Atomically check-and-claim the UID slot to prevent two threads from loading the same resource.
	// We insert a nullptr placeholder under the lock, load outside it, then replace.
	bool needsLoad = false;
	{
		std::lock_guard<std::mutex> lock(resourcesMutex);
		if (resources.find(metaFileData.uid) == resources.end())
		{
			resources[metaFileData.uid] = nullptr; // claim slot; marks "in progress"
			needsLoad = true;
		}
	}

	if (needsLoad)
	{
		NOUS_INFO("Resource with UID %u does not exist. Creating new instance...", metaFileData.uid);

		Resource* resource = InstantiateResource(metaFileData.resourceType);
		if (resource == nullptr)
		{
			NOUS_ERROR("CreateResource ERROR: Failed to instantiate resource of type %s.",
					   Resource::GetLibraryExtensionFromType(metaFileData.resourceType).c_str());
			// Remove the placeholder so the slot is available again.
			std::lock_guard<std::mutex> lock(resourcesMutex);
			resources.erase(metaFileData.uid);
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
			std::lock_guard<std::mutex> lock(resourcesMutex);
			resources.erase(metaFileData.uid);
			return nullptr;
		}

		{
			std::lock_guard<std::mutex> lock(resourcesMutex);
			resources[metaFileData.uid] = resource;
		}

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

		// Spin-wait if another thread is still loading (slot is claimed but pointer is nullptr).
		Resource* existingResource = nullptr;
		while (true)
		{
			{
				std::lock_guard<std::mutex> lock(resourcesMutex);
				existingResource = resources.count(metaFileData.uid) ? resources.at(metaFileData.uid) : nullptr;
			}
			if (existingResource != nullptr) break;
			NOUS_Multithreading::NOUS_Thread::SleepMS(1); // yield while other thread loads
		}

		existingResource->IncreaseReferenceCount();
		existingResource->Validate();

		NOUS_INFO("Existing resource retrieved successfully (Name: %s, RefCount: %u).",
				  existingResource->GetName().c_str(), existingResource->GetReferenceCount());
		NOUS_INFO("========================================");

		return existingResource;
	}
}

Resource* ModuleResourceManager::CreateResourceFromLibrary(UID uid, ResourceType type,
                                                            const std::string& name,
                                                            const std::string& assetsPath,
                                                            const std::string& libraryPath)
{
	bool needsLoad = false;
	{
		std::lock_guard<std::mutex> lock(resourcesMutex);
		if (resources.find(uid) == resources.end())
		{
			resources[uid] = nullptr; // claim slot
			needsLoad = true;
		}
	}

	if (needsLoad)
	{
		Resource* resource = InstantiateResource(type);
		if (!resource)
		{
			std::lock_guard<std::mutex> lock(resourcesMutex);
			resources.erase(uid);
			return nullptr;
		}

		resource->SetName(name);
		resource->SetUID(uid);
		resource->SetType(type);
		resource->SetAssetsPath(assetsPath);
		resource->SetLibraryPath(libraryPath);

		if (!ImporterManager::Load(type, libraryPath, resource))
		{
			NOUS_ERROR("CreateResourceFromLibrary: failed to load '%s' from '%s'", name.c_str(), libraryPath.c_str());
			std::lock_guard<std::mutex> lock(resourcesMutex);
			resources.erase(uid);
			return nullptr;
		}

		{
			std::lock_guard<std::mutex> lock(resourcesMutex);
			resources[uid] = resource;
		}

		resource->IncreaseReferenceCount();
		resource->Validate();
		return resource;
	}
	else
	{
		Resource* existing = nullptr;
		while (true)
		{
			{
				std::lock_guard<std::mutex> lock(resourcesMutex);
				existing = resources.count(uid) ? resources.at(uid) : nullptr;
			}
			if (existing) break;
			NOUS_Multithreading::NOUS_Thread::SleepMS(1);
		}
		existing->IncreaseReferenceCount();
		return existing;
	}
}

ResourceMesh* ModuleResourceManager::RequestOrCreateSubMeshResourceFromLibrary(
    const std::string& libraryPath, int32_t submeshIndex)
{
	// Use hash(libraryPath) as a stable synthetic base UID for dedup.
	const UID baseUID = static_cast<UID>(std::hash<std::string>{}(libraryPath) & 0xFFFFFFFF);
	const auto key = std::make_pair(baseUID, submeshIndex);

	{
		std::lock_guard<std::mutex> lock(resourcesMutex);
		auto mapIt = m_submeshUIDMap.find(key);
		if (mapIt != m_submeshUIDMap.end())
		{
			auto resIt = resources.find(mapIt->second);
			if (resIt != resources.end() && resIt->second)
			{
				resIt->second->IncreaseReferenceCount();
				return down_cast<ResourceMesh*>(resIt->second);
			}
			m_submeshUIDMap.erase(mapIt);
		}
	}

	const auto hierarchy = ImporterMesh::LoadHierarchy(libraryPath);
	if (submeshIndex < 0 || submeshIndex >= static_cast<int32_t>(hierarchy.size()))
	{
		NOUS_ERROR("RequestOrCreateSubMeshResourceFromLibrary: index %d out of range for '%s'",
		    submeshIndex, libraryPath.c_str());
		return nullptr;
	}

	const SubMeshData& sub = hierarchy[static_cast<size_t>(submeshIndex)];
	ResourceMesh* mesh = NOUS_NEW<ResourceMesh>(MemoryTag::RESOURCE_MESH);
	mesh->SetName(sub.name);
	mesh->SetType(ResourceType::MESH);
	mesh->SetLibraryPath(libraryPath);

	mesh->vertices = sub.vertices;
	mesh->indices.assign(sub.indices.begin(), sub.indices.end());

	if (!mGPUFactory->CreateGeometry(
	    mesh->vertices.size(), mesh->vertices.data(),
	    mesh->indices.size(), mesh->indices.data(), mesh))
	{
		NOUS_ERROR("RequestOrCreateSubMeshResourceFromLibrary: GPU upload failed for submesh %d of '%s'",
		    submeshIndex, libraryPath.c_str());
		NOUS_DELETE(mesh, MemoryTag::RESOURCE_MESH);
		return nullptr;
	}

	UID uid;
	{
		std::lock_guard<std::mutex> lock(resourcesMutex);
		do { uid = static_cast<UID>(Random::Generate()); }
		while (uid == 0 || resources.count(uid));

		resources[uid] = mesh;
		m_submeshUIDMap[key] = uid;
	}

	mesh->SetUID(uid);
	mesh->IncreaseReferenceCount();
	mesh->Validate();
	return mesh;
}

bool ModuleResourceManager::UnloadResource(const UID& UID)
{
	if (!ResourceExists(UID))
		return false;

	Resource* tmpResource = resources[UID];
	if (!tmpResource) return false; // still loading (placeholder); ignore
	tmpResource->DecreaseReferenceCount();

	// Only destroy GPU resources and delete the object when the last reference is
	// released. Defer to PreUpdate so we never destroy GPU resources mid-frame
	// (between command recording and vkQueueSubmit).
	if (tmpResource->GetReferenceCount() == 0)
	{
		std::lock_guard<std::mutex> lock(m_PendingUnloadsMutex);
		m_PendingUnloads.push_back(UID);
	}

	return true;
}

Resource* ModuleResourceManager::RequestResource(const UID& uid)
{
	std::lock_guard<std::mutex> lock(resourcesMutex);
	auto it = resources.find(uid);
	if (it == resources.end() || !it->second) return nullptr;
	// Resource is already fully loaded (GPU data is valid). Just bump the ref count
	// and return the existing pointer — do NOT re-call ImporterManager::Load, which
	// would re-upload GPU data and overwrite internalID / internalData.
	it->second->IncreaseReferenceCount();
	return it->second;
}

void ModuleResourceManager::AddResource(const UID& uid, Resource*& resource)
{
	std::lock_guard<std::mutex> lock(resourcesMutex);  // Multi-threading
	resources[uid] = resource;
}

void ModuleResourceManager::ClearResources()
{
    // Destroy shaders FIRST — DestroyShader() frees descriptor sets that reference
    // texture imageViews. If textures are destroyed first, the imageViews are freed
    // while descriptor sets still hold references → VUID-vkDestroyImageView-01026.
    for (auto& [UID, Resource] : resources)
    {
        if (!Resource || Resource->GetType() != ResourceType::SHADER) continue;
        ImporterManager::Unload(ResourceType::SHADER, Resource);
        NOUS_DELETE(Resource, MemoryTag::RESOURCE_SHADER);
    }

    // Destroy remaining resources (mesh, texture, material) in any order.
    for (auto& [UID, Resource] : resources)
    {
        if (!Resource || Resource->GetType() == ResourceType::SHADER) continue;
        ImporterManager::Unload(Resource->GetType(), Resource);

        switch (Resource->GetType())
        {
            case ResourceType::MESH:     NOUS_DELETE(Resource, MemoryTag::RESOURCE_MESH);     break;
            case ResourceType::MATERIAL: NOUS_DELETE(Resource, MemoryTag::RESOURCE_MATERIAL); break;
            case ResourceType::TEXTURE:  NOUS_DELETE(Resource, MemoryTag::RESOURCE_TEXTURE);  break;
            default: break;
        }
    }

	resources.clear();

    if (mDefaultTexture)
    {
        mGPUFactory->DestroyTexture(mDefaultTexture);
        NOUS_DELETE(mDefaultTexture, MemoryTag::RESOURCE_TEXTURE);
        mDefaultTexture = nullptr;
    }

    if (mDefaultMaterial)
    {
        mGPUFactory->DestroyMaterial(mDefaultMaterial);
        NOUS_DELETE(mDefaultMaterial, MemoryTag::RESOURCE_MATERIAL);
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

bool ModuleResourceManager::GetAssetMetaData(const std::string& assetsPath, MetaFileData& outData)
{
    return ReadMetaFile(assetsPath + ".meta", outData);
}

ResourceMesh* ModuleResourceManager::RequestOrCreateSubMeshResource(const std::string& assetsPath,
                                                                      int32_t submeshIndex)
{
    // Read meta to resolve the base UID and library path.
    MetaFileData metaData;
    if (!ReadMetaFile(assetsPath + ".meta", metaData))
    {
        NOUS_ERROR("RequestOrCreateSubMeshResource: missing meta for %s", assetsPath.c_str());
        return nullptr;
    }

    const auto key = std::make_pair(metaData.uid, submeshIndex);

    // Fast path: sub-resource already loaded this session.
    {
        std::lock_guard<std::mutex> lock(resourcesMutex);
        auto mapIt = m_submeshUIDMap.find(key);
        if (mapIt != m_submeshUIDMap.end())
        {
            auto resIt = resources.find(mapIt->second);
            if (resIt != resources.end() && resIt->second)
            {
                resIt->second->IncreaseReferenceCount();
                return down_cast<ResourceMesh*>(resIt->second);
            }
            // Stale entry (resource was destroyed) — remove and recreate below.
            m_submeshUIDMap.erase(mapIt);
        }
    }

    // Load the full hierarchy to pick the requested submesh.
    const auto hierarchy = ImporterMesh::LoadHierarchy(metaData.libraryPath);
    if (submeshIndex < 0 || submeshIndex >= static_cast<int32_t>(hierarchy.size()))
    {
        NOUS_ERROR("RequestOrCreateSubMeshResource: index %d out of range (count=%zu) for %s",
            submeshIndex, hierarchy.size(), assetsPath.c_str());
        return nullptr;
    }

    const SubMeshData& sub = hierarchy[static_cast<size_t>(submeshIndex)];

    // Build the ResourceMesh.
    ResourceMesh* mesh = NOUS_NEW<ResourceMesh>(MemoryTag::RESOURCE_MESH);
    mesh->SetName(sub.name);
    mesh->SetType(ResourceType::MESH);
    mesh->SetAssetsPath(assetsPath);
    mesh->SetLibraryPath(metaData.libraryPath);

    mesh->vertices = sub.vertices;
    mesh->indices.assign(sub.indices.begin(), sub.indices.end());

    // Upload geometry to the GPU.
    if (!mGPUFactory->CreateGeometry(
        mesh->vertices.size(), mesh->vertices.data(),
        mesh->indices.size(), mesh->indices.data(), mesh))
    {
        NOUS_ERROR("RequestOrCreateSubMeshResource: GPU upload failed for submesh %d of %s",
            submeshIndex, assetsPath.c_str());
        NOUS_DELETE(mesh, MemoryTag::RESOURCE_MESH);
        return nullptr;
    }

    // Assign a unique UID, register in the resource map and the sub-resource map.
    UID uid;
    {
        std::lock_guard<std::mutex> lock(resourcesMutex);
        do { uid = static_cast<UID>(Random::Generate()); }
        while (uid == 0 || resources.count(uid));

        resources[uid]         = mesh;
        m_submeshUIDMap[key]   = uid;
    }

    mesh->SetUID(uid);
    mesh->IncreaseReferenceCount();
    mesh->Validate();

    return mesh;
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