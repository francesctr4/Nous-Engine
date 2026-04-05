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

#include "Engine/Systems/ResourceManager/Importer/IImporterManager.h"
#include "Engine/Systems/ResourceManager/Importer/ImporterManager.h"
#include "Engine/Systems/ResourceManager/Importer/ImporterMesh/include/ImporterMesh.h"
#include "Engine/NOUS_Multithreading/NOUS_Thread/include/NOUS_Thread.h"
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"

#include <future>
#include <parson.h>
#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_RESOURCEMANAGER;

ModuleResourceManager::ModuleResourceManager(EventSystem* eventSystem, NOUS_Multithreading::NOUS_JobSystem* jobSystem,
                                             IImporterManager* importerManager)
    : Module(eventSystem, jobSystem), mImporterManager(importerManager)
{
	eventSystem->Subscribe(EventType::DROP_FILE, this);
}

ModuleResourceManager::~ModuleResourceManager()
{

}

bool ModuleResourceManager::Awake()
{
	mImporterManager->Init(this);

	// Always ensure directories exist — idempotent, safe to call in any mode.
	EnsureLibraryDirectories();

	return true;
}

void ModuleResourceManager::ScanAndImportAssets()
{
	// Scan Assets/ on startup. ImportFile is a cheap no-op for assets
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
	// Default Texture (CPU)
	// -----------------------
	// Build a checkerboard pixel buffer and store it on the resource for deferred
	// GPU upload.  The GPU upload itself happens in ModuleRenderer3D::Start() which
	// runs after this module and drains TakePendingUploads().
	NOUS_INFO("Queuing default checkerboard texture for GPU upload...");

	const uint32 texDimension = 256;
	const uint32 channels = 4;
	const uint32 pixelCount = texDimension * texDimension;
	const uint32 squareSize = 16;

	// Reserved UIDs for built-in fallback textures. Placed at the top of the uint32
	// range so they cannot collide with randomly-generated asset UIDs in .meta files.
	// These UIDs are what the descriptor lazy-write dedup (WriteInstanceSampler) keys
	// off via Resource::GetUID(), so each fallback must be uniquely identifiable.
	mDefaultTexture = NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE);
	mDefaultTexture->SetUID(INVALID_ID - 1);
	mDefaultTexture->SetName("DefaultTexture");
	mDefaultTexture->width        = texDimension;
	mDefaultTexture->height       = texDimension;
	mDefaultTexture->channelCount = channels;

	mDefaultTexture->pixelData.resize(pixelCount * channels, 255);
	for (uint32_t row = 0; row < texDimension; ++row)
	{
		for (uint32_t col = 0; col < texDimension; ++col)
		{
			const uint32_t indexBpp = ((row * texDimension) + col) * channels;
			const bool isWhite = ((row / squareSize) % 2 == (col / squareSize) % 2);

			mDefaultTexture->pixelData[indexBpp + 0] = isWhite ? 255 : 0;
			mDefaultTexture->pixelData[indexBpp + 1] = isWhite ? 255 : 0;
			mDefaultTexture->pixelData[indexBpp + 2] = isWhite ? 255 : 255;
			mDefaultTexture->pixelData[indexBpp + 3] = 255;
		}
	}
	mDefaultTexture->SetState(ResourceState::CPU_READY);

	// -----------------------
	// White Texture (CPU)
	// -----------------------
	// 1×1 pure-white (1,1,1,1). Neutral identity for multiplicative slots (specular
	// strength, shininess, AO) — multiplying by 1 has no effect.
	mWhiteTexture = NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE);
	mWhiteTexture->SetUID(INVALID_ID - 2);
	mWhiteTexture->SetName("WhiteTexture");
	mWhiteTexture->width        = 1;
	mWhiteTexture->height       = 1;
	mWhiteTexture->channelCount = 4;
	mWhiteTexture->pixelData    = { 255, 255, 255, 255 };
	mWhiteTexture->SetState(ResourceState::CPU_READY);

	// -----------------------
	// Black Texture (CPU)
	// -----------------------
	// 1×1 pure-black (0,0,0,1). Neutral identity for additive slots (emissive) —
	// adding 0 has no effect.
	mBlackTexture = NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE);
	mBlackTexture->SetUID(INVALID_ID - 3);
	mBlackTexture->SetName("BlackTexture");
	mBlackTexture->width        = 1;
	mBlackTexture->height       = 1;
	mBlackTexture->channelCount = 4;
	mBlackTexture->pixelData    = { 0, 0, 0, 255 };
	mBlackTexture->SetState(ResourceState::CPU_READY);

	// -----------------------
	// Flat Normal Texture (CPU)
	// -----------------------
	// 1×1 tangent-space flat normal (128,128,255,255). Decoded as (0,0,1) in [-1,1],
	// which after TBN multiplication gives the unperturbed geometry normal. Using the
	// white texture as a normal-map fallback would produce a 45° tilt instead.
	mFlatNormalTexture = NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE);
	mFlatNormalTexture->SetUID(INVALID_ID - 4);
	mFlatNormalTexture->SetName("FlatNormalTexture");
	mFlatNormalTexture->width        = 1;
	mFlatNormalTexture->height       = 1;
	mFlatNormalTexture->channelCount = 4;
	mFlatNormalTexture->pixelData    = { 128, 128, 255, 255 };
	mFlatNormalTexture->SetState(ResourceState::CPU_READY);

	// -----------------------
	// Default Material (CPU)
	// -----------------------
	mDefaultMaterial = NOUS_NEW<ResourceMaterial>(MemoryTag::RESOURCE_MATERIAL);
	mDefaultMaterial->SetName("DefaultMaterial");
	mDefaultMaterial->diffuseColor = glm::vec4(1.0f);
	mDefaultMaterial->textureMaps["diffuseSampler"].texture = mDefaultTexture;
	mDefaultMaterial->SetState(ResourceState::CPU_READY);

	// Push to the upload queue.  Textures must come before the material so they
	// are GPU_READY before CreateMaterial samples from them.
	{
		std::lock_guard<std::mutex> lock(m_pendingUploadsMutex);
		m_pendingUploads.emplace_back(ResourceType::TEXTURE,  mDefaultTexture);
		m_pendingUploads.emplace_back(ResourceType::TEXTURE,  mWhiteTexture);
		m_pendingUploads.emplace_back(ResourceType::TEXTURE,  mBlackTexture);
		m_pendingUploads.emplace_back(ResourceType::TEXTURE,  mFlatNormalTexture);
		m_pendingUploads.emplace_back(ResourceType::MATERIAL, mDefaultMaterial);
	}

	return true;
}

UpdateStatus ModuleResourceManager::PreUpdate(float dt)
{
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
			mImporterManager->Import(metaFileData.resourceType, metaFileData);

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
				mImporterManager->Import(metaFileData.resourceType, metaFileData);

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

					mImporterManager->Import(metaFileData.resourceType, metaFileData);
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
	// Must hold resourcesMutex — RequestOrCreateSubMeshResource reads/writes this
	// map under the same lock from worker threads.
	{
		std::lock_guard<std::mutex> lock(resourcesMutex);
		for (auto it = m_submeshUIDMap.begin(); it != m_submeshUIDMap.end(); )
		{
			if (it->second == uid)
				it = m_submeshUIDMap.erase(it);
			else
				++it;
		}
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

	// NOTE: resources.erase(uid) is intentionally NOT here.
	// EvictResource removes the entry under resourcesMutex before calling DeleteResource,
	// so the map is clean before we reach this point.
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

		if (!mImporterManager->Deserialize(metaFileData.resourceType, metaFileData.libraryPath, resource))
		{
			NOUS_ERROR("CreateResource ERROR: Failed to deserialize resource from library: %s",
					   metaFileData.libraryPath.c_str());
			std::lock_guard<std::mutex> lock(resourcesMutex);
			resources.erase(metaFileData.uid);
			return nullptr;
		}

		resource->SetState(ResourceState::CPU_READY);

		{
			std::lock_guard<std::mutex> lock(resourcesMutex);
			resources[metaFileData.uid] = resource;
		}

		{
			std::lock_guard<std::mutex> lock(m_pendingUploadsMutex);
			m_pendingUploads.emplace_back(metaFileData.resourceType, resource);
		}

		resource->IncreaseReferenceCount();
		resource->Validate();

		NOUS_INFO("Resource deserialized and queued for GPU upload.");
		NOUS_INFO("Reference count: %u", resource->GetReferenceCount());
		NOUS_INFO("========================================");

		return resource;
	}
	else
	{
		NOUS_INFO("Resource with UID %u already exists. Requesting existing instance...", metaFileData.uid);

		// Spin-wait if another thread is still loading (slot is claimed but pointer is nullptr).
		// The IncreaseReferenceCount MUST happen inside the same lock acquisition as the pointer
		// read so EvictResource cannot free the object between the two operations.
		Resource* existingResource = nullptr;
		while (true)
		{
			{
				std::lock_guard<std::mutex> lock(resourcesMutex);
				auto it = resources.find(metaFileData.uid);
				if (it == resources.end())
				{
					// Entry was evicted and erased from the map while we were waiting.
					// Fall through to the creation path by returning nullptr here — the
					// caller (CMaterial::Deserialize etc.) will re-try via CreateResource.
					NOUS_WARN("Resource UID %u was evicted before we could acquire it; caller should retry.",
					          metaFileData.uid);
					return nullptr;
				}
				existingResource = it->second;
				if (existingResource != nullptr)
				{
					// Bump refcount under the lock to close the eviction race window.
					existingResource->IncreaseReferenceCount();
					break;
				}
			}
			NOUS_Multithreading::NOUS_Thread::SleepMS(1); // yield while other thread loads
		}

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

		if (!mImporterManager->Deserialize(type, libraryPath, resource))
		{
			NOUS_ERROR("CreateResourceFromLibrary: failed to deserialize '%s' from '%s'", name.c_str(), libraryPath.c_str());
			DeleteResource(resource);
			std::lock_guard<std::mutex> lock(resourcesMutex);
			resources.erase(uid);
			return nullptr;
		}

		resource->SetState(ResourceState::CPU_READY);

		{
			std::lock_guard<std::mutex> lock(resourcesMutex);
			resources[uid] = resource;
		}

		{
			std::lock_guard<std::mutex> lock(m_pendingUploadsMutex);
			m_pendingUploads.emplace_back(type, resource);
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
				auto it = resources.find(uid);
				if (it == resources.end())
					return nullptr; // evicted; caller should retry
				existing = it->second;
				if (existing != nullptr)
				{
					existing->IncreaseReferenceCount(); // under lock — closes the eviction race
					break;
				}
			}
			NOUS_Multithreading::NOUS_Thread::SleepMS(1);
		}
		return existing;
	}
}

ResourceMesh* ModuleResourceManager::RequestOrCreateSubMeshResourceFromLibrary(
    const std::string& libraryPath, int32_t submeshIndex, const std::string& assetsPath)
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
				// Cache hit: back-fill assetsPath if the earlier caller didn't have it
				// (e.g. GAME-mode load) but this caller does (EDITOR-mode scene load).
				if (!assetsPath.empty() && resIt->second->GetAssetsPath().empty())
					resIt->second->SetAssetsPath(assetsPath);
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
	if (!assetsPath.empty())
		mesh->SetAssetsPath(assetsPath);

	mesh->vertices = sub.vertices;
	mesh->indices.assign(sub.indices.begin(), sub.indices.end());
	mesh->SetState(ResourceState::CPU_READY);

	UID uid;
	{
		std::lock_guard<std::mutex> lock(resourcesMutex);
		do { uid = static_cast<UID>(Random::Generate()); }
		while (uid == 0 || resources.count(uid));

		resources[uid] = mesh;
		m_submeshUIDMap[key] = uid;
	}

	{
		std::lock_guard<std::mutex> lock(m_pendingUploadsMutex);
		m_pendingUploads.emplace_back(ResourceType::MESH, mesh);
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

	// Defer GPU Release + CPU Evict to the Renderer's PreUpdate so we never
	// destroy GPU resources mid-frame (between command recording and vkQueueSubmit).
	if (tmpResource->GetReferenceCount() == 0)
	{
		std::lock_guard<std::mutex> lock(m_pendingReleasesMutex);
		m_pendingReleases.emplace_back(tmpResource->GetType(), tmpResource);
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

void ModuleResourceManager::ClearResources(IGPUResourceFactory* gpu)
{
    // Destroy shaders FIRST — descriptor sets reference texture image views;
    // freeing textures first would leave dangling view references in the sets.
    // VUID-vkDestroyImageView-01026
    for (auto& [uid, res] : resources)
    {
        if (!res || res->GetType() != ResourceType::SHADER) continue;
        if (res->GetState() == ResourceState::GPU_READY)
            gpu->DestroyShader(down_cast<ResourceShader*>(res));
        mImporterManager->Evict(ResourceType::SHADER, res);
        NOUS_DELETE(res, MemoryTag::RESOURCE_SHADER);
    }

    // Materials next — destroy GPU handle, then clear the texture pointer directly
    // (do NOT call UnloadResource here; the texture will be destroyed in the next pass).
    for (auto& [uid, res] : resources)
    {
        if (!res || res->GetType() != ResourceType::MATERIAL) continue;
        if (res->GetState() == ResourceState::GPU_READY)
            gpu->DestroyMaterial(down_cast<ResourceMaterial*>(res));
        auto* mat = down_cast<ResourceMaterial*>(res);
        for (auto& [name, map] : mat->textureMaps)
            map.texture = nullptr;
        NOUS_DELETE(res, MemoryTag::RESOURCE_MATERIAL);
    }

    // Textures
    for (auto& [uid, res] : resources)
    {
        if (!res || res->GetType() != ResourceType::TEXTURE) continue;
        if (res->GetState() == ResourceState::GPU_READY)
            gpu->DestroyTexture(down_cast<ResourceTexture*>(res));
        mImporterManager->Evict(ResourceType::TEXTURE, res);
        NOUS_DELETE(res, MemoryTag::RESOURCE_TEXTURE);
    }

    // Meshes
    for (auto& [uid, res] : resources)
    {
        if (!res || res->GetType() != ResourceType::MESH) continue;
        if (res->GetState() == ResourceState::GPU_READY)
            gpu->DestroyGeometry(down_cast<ResourceMesh*>(res));
        mImporterManager->Evict(ResourceType::MESH, res);
        NOUS_DELETE(res, MemoryTag::RESOURCE_MESH);
    }

    resources.clear();
    m_submeshUIDMap.clear();

    if (mDefaultTexture)
    {
        if (mDefaultTexture->GetState() == ResourceState::GPU_READY)
            gpu->DestroyTexture(mDefaultTexture);
        NOUS_DELETE(mDefaultTexture, MemoryTag::RESOURCE_TEXTURE);
        mDefaultTexture = nullptr;
    }

    if (mWhiteTexture)
    {
        if (mWhiteTexture->GetState() == ResourceState::GPU_READY)
            gpu->DestroyTexture(mWhiteTexture);
        NOUS_DELETE(mWhiteTexture, MemoryTag::RESOURCE_TEXTURE);
        mWhiteTexture = nullptr;
    }

    if (mBlackTexture)
    {
        if (mBlackTexture->GetState() == ResourceState::GPU_READY)
            gpu->DestroyTexture(mBlackTexture);
        NOUS_DELETE(mBlackTexture, MemoryTag::RESOURCE_TEXTURE);
        mBlackTexture = nullptr;
    }

    if (mFlatNormalTexture)
    {
        if (mFlatNormalTexture->GetState() == ResourceState::GPU_READY)
            gpu->DestroyTexture(mFlatNormalTexture);
        NOUS_DELETE(mFlatNormalTexture, MemoryTag::RESOURCE_TEXTURE);
        mFlatNormalTexture = nullptr;
    }

    if (mDefaultMaterial)
    {
        if (mDefaultMaterial->GetState() == ResourceState::GPU_READY)
            gpu->DestroyMaterial(mDefaultMaterial);
        NOUS_DELETE(mDefaultMaterial, MemoryTag::RESOURCE_MATERIAL);
        mDefaultMaterial = nullptr;
    }

    // Discard any stale queue entries — all resources are destroyed above.
    { std::lock_guard<std::mutex> lock(m_pendingUploadsMutex);  m_pendingUploads.clear(); }
    { std::lock_guard<std::mutex> lock(m_pendingReleasesMutex); m_pendingReleases.clear(); }
}

std::vector<std::pair<ResourceType, Resource*>> ModuleResourceManager::TakePendingUploads()
{
    std::vector<std::pair<ResourceType, Resource*>> result;
    std::lock_guard<std::mutex> lock(m_pendingUploadsMutex);
    std::swap(result, m_pendingUploads);
    return result;
}

std::vector<std::pair<ResourceType, Resource*>> ModuleResourceManager::TakePendingReleases()
{
    std::vector<std::pair<ResourceType, Resource*>> result;
    std::lock_guard<std::mutex> lock(m_pendingReleasesMutex);
    std::swap(result, m_pendingReleases);
    return result;
}

bool ModuleResourceManager::EvictResource(ResourceType type, Resource* resource)
{
    // Final refcount check + map erasure under the same lock so that a concurrent
    // CreateResource thread cannot bump the refcount between our check and the delete.
    {
        std::lock_guard<std::mutex> lock(resourcesMutex);
        if (resource->GetReferenceCount() > 0)
        {
            // Re-acquired since TakePendingReleases. ImporterManager::Release already
            // freed the GPU resources, so re-queue for upload to restore GPU state.
            std::lock_guard<std::mutex> uploadLock(m_pendingUploadsMutex);
            m_pendingUploads.emplace_back(type, resource);
            return false;
        }
        resources.erase(resource->GetUID());
    }

    mImporterManager->Evict(type, resource);
    DeleteResource(resource);
    return true;
}

ResourceTexture *ModuleResourceManager::GetDefaultTexture() const
{
    return mDefaultTexture;
}

ResourceTexture *ModuleResourceManager::GetWhiteTexture() const
{
    return mWhiteTexture;
}

ResourceTexture *ModuleResourceManager::GetBlackTexture() const
{
    return mBlackTexture;
}

ResourceTexture *ModuleResourceManager::GetFlatNormalTexture() const
{
    return mFlatNormalTexture;
}

ResourceMaterial *ModuleResourceManager::GetDefaultMaterial() const
{
    return mDefaultMaterial;
}

Resource* ModuleResourceManager::GetLoadedResource(UID uid)
{
    std::lock_guard<std::mutex> lock(resourcesMutex);
    auto it = resources.find(uid);
    if (it == resources.end() || it->second == nullptr)
        return nullptr;
    return it->second;
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
    mesh->SetState(ResourceState::CPU_READY);

    // Assign a unique UID, register in the resource map and the sub-resource map.
    UID uid;
    {
        std::lock_guard<std::mutex> lock(resourcesMutex);
        do { uid = static_cast<UID>(Random::Generate()); }
        while (uid == 0 || resources.count(uid));

        resources[uid]       = mesh;
        m_submeshUIDMap[key] = uid;
    }

    {
        std::lock_guard<std::mutex> lock(m_pendingUploadsMutex);
        m_pendingUploads.emplace_back(ResourceType::MESH, mesh);
    }

    mesh->SetUID(uid);
    mesh->IncreaseReferenceCount();
    mesh->Validate();

    return mesh;
}

std::vector<std::future<void>> ModuleResourceManager::PreloadSceneResourcesAsync(
    NOUS_Multithreading::NOUS_JobSystem* jobSystem,
    const std::string& sceneFilePath)
{
    std::vector<std::future<void>> futures;

    JSON_Value* root = json_parse_file(sceneFilePath.c_str());
    if (!root)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "PreloadSceneResourcesAsync: failed to parse '%s'", sceneFilePath.c_str());
        return futures;
    }

    JSON_Object* rootObj    = json_value_get_object(root);
    JSON_Array*  gameObjects = json_object_get_array(rootObj, "GameObjects");

    if (!gameObjects)
    {
        json_value_free(root);
        return futures;
    }

    // Collect every unique CMesh resource request in the scene file.
    // Key: (assetPath, submeshIndex) — same deduplication the ResourceManager uses internally.
    struct MeshRequest
    {
        std::string assetPath;
        std::string libraryPath;
        UID         uid          = 0;
        int32_t     submeshIndex = -1;
    };

    std::map<std::pair<std::string, int32_t>, MeshRequest> uniqueRequests;

    const size_t goCount = json_array_get_count(gameObjects);
    for (size_t i = 0; i < goCount; ++i)
    {
        JSON_Object* goObj     = json_array_get_object(gameObjects, i);
        JSON_Array*  comps     = json_object_get_array(goObj, "components");
        if (!comps) continue;

        const size_t compCount = json_array_get_count(comps);
        for (size_t j = 0; j < compCount; ++j)
        {
            JSON_Object* compObj = json_array_get_object(comps, j);
            const char*  type    = json_object_get_string(compObj, "type");
            if (!type || std::strcmp(type, "CMesh") != 0) continue;

            const char* assetPath = json_object_get_string(compObj, "assetPath");
            if (!assetPath || assetPath[0] == '\0') continue;

            const char*        libRaw   = json_object_get_string(compObj, "libraryPath");
            const JSON_Value*  uidVal   = json_object_get_value(compObj, "resourceUID");
            const JSON_Value*  subIdxV  = json_object_get_value(compObj, "submeshIndex");

            MeshRequest req;
            req.assetPath    = assetPath;
            req.libraryPath  = libRaw ? libRaw : "";
            req.uid          = uidVal   ? static_cast<UID>(json_value_get_number(uidVal))   : 0;
            req.submeshIndex = subIdxV  ? static_cast<int32_t>(json_value_get_number(subIdxV)) : -1;

            uniqueRequests[{req.assetPath, req.submeshIndex}] = std::move(req);
        }
    }

    json_value_free(root);

    // Submit one parallel Deserialize job per unique resource.
    futures.reserve(uniqueRequests.size());

    for (const auto& [key, req] : uniqueRequests)
    {
        auto prom = std::make_shared<std::promise<void>>();
        futures.push_back(prom->get_future());

        jobSystem->SubmitJob([this, req, prom]()
        {
            Resource* res = nullptr;
            if (req.submeshIndex >= 0)
            {
                if (!req.libraryPath.empty())
                    res = RequestOrCreateSubMeshResourceFromLibrary(req.libraryPath, req.submeshIndex, req.assetPath);
                else
                    res = RequestOrCreateSubMeshResource(req.assetPath, req.submeshIndex);
            }
            else
            {
                if (!req.libraryPath.empty() && req.uid != 0)
                    res = CreateResourceFromLibrary(req.uid, ResourceType::MESH,
                        std::filesystem::path(req.assetPath).filename().string(),
                        req.assetPath, req.libraryPath);
                else
                    res = CreateResource(req.assetPath);
            }
            // Release the preload's reference. The resource stays in the map so
            // CMesh::Deserialize() hits the fast path, but the preload does not
            // hold an extra ref that would prevent eviction.
            if (res) res->DecreaseReferenceCount();
            prom->set_value();
        }, "Preload: " + req.assetPath);
    }

    NOUS_INFO_C(CURRENT_CHANNEL,
        "PreloadSceneResourcesAsync: submitted %zu parallel jobs for '%s'",
        uniqueRequests.size(), sceneFilePath.c_str());

    return futures;
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