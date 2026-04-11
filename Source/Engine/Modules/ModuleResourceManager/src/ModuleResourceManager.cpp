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
#include <ranges>
#include <utility>

#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"

#include <unordered_map>

// ---------------------------------------------------------------------------
// Resource factory table — one entry per ResourceType.
// Adding a new type: add one entry here; no other edits required.
// ---------------------------------------------------------------------------
namespace
{
    struct ResourceFactory
    {
        Resource* (*create)();
        void      (*destroy)(Resource*);
    };

    const std::unordered_map<ResourceType, ResourceFactory> k_ResourceFactories =
    {
        { ResourceType::MESH,
          { []() -> Resource* { return NOUS_NEW<ResourceMesh>(MemoryTag::RESOURCE_MESH); },
            [](Resource* r)   { NOUS_DELETE(r, MemoryTag::RESOURCE_MESH); } }},
        { ResourceType::MATERIAL,
          { []() -> Resource* { return NOUS_NEW<ResourceMaterial>(MemoryTag::RESOURCE_MATERIAL); },
            [](Resource* r)   { NOUS_DELETE(r, MemoryTag::RESOURCE_MATERIAL); } }},
        { ResourceType::TEXTURE,
          { []() -> Resource* { return NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE); },
            [](Resource* r)   { NOUS_DELETE(r, MemoryTag::RESOURCE_TEXTURE); } }},
        { ResourceType::SHADER,
          { []() -> Resource* { return NOUS_NEW<ResourceShader>(MemoryTag::RESOURCE_SHADER); },
            [](Resource* r)   { NOUS_DELETE(r, MemoryTag::RESOURCE_SHADER); } }},
    };
}

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_RESOURCEMANAGER;

ModuleResourceManager::ModuleResourceManager(EventSystem* eventSystem, NOUS_Multithreading::NOUS_JobSystem* jobSystem,
                                             IImporterManager* importerManager)
    : Module(eventSystem, jobSystem), mImporterManager(importerManager)
{
	eventSystem->Subscribe(EventType::DROP_FILE, this);
}

ModuleResourceManager::~ModuleResourceManager() = default;

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
	NOUS_INFO("Queuing built-in textures and default material for GPU upload...");
	CreateBuiltinTextures();
	CreateBuiltinMaterial();

	// Push to the upload queue. Textures must come before the material so they
	// are GPU_READY before CreateMaterial samples from them.
	{
		using enum ResourceType;
		std::scoped_lock lock(m_pendingUploadsMutex);
		m_pendingUploads.emplace_back(TEXTURE,  mDefaultTexture);
		m_pendingUploads.emplace_back(TEXTURE,  mWhiteTexture);
		m_pendingUploads.emplace_back(TEXTURE,  mBlackTexture);
		m_pendingUploads.emplace_back(TEXTURE,  mFlatNormalTexture);
		m_pendingUploads.emplace_back(MATERIAL, mDefaultMaterial);
	}

	return true;
}

void ModuleResourceManager::CreateBuiltinTextures()
{
	// Reserved UIDs for built-in fallback textures. Placed at the top of the uint32
	// range so they cannot collide with randomly-generated asset UIDs in .meta files.
	// These UIDs are what the descriptor lazy-write dedup (WriteInstanceSampler) keys
	// off via Resource::GetUID(), so each fallback must be uniquely identifiable.

	// Default Texture — 256×256 checkerboard (CPU)
	// Build a checkerboard pixel buffer and store it on the resource for deferred
	// GPU upload. The GPU upload itself happens in ModuleRenderer3D::Start() which
	// runs after this module and drains TakePendingUploads().
	constexpr uint32 texDimension = 256;
	constexpr uint32 channels     = 4;
	constexpr uint32 pixelCount   = texDimension * texDimension;

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
			constexpr uint32 squareSize = 16;
			const uint32_t   indexBpp   = (row * texDimension + col) * channels;
			const bool       isWhite    = row / squareSize % 2 == col / squareSize % 2;
			mDefaultTexture->pixelData[indexBpp + 0] = isWhite ? 255 : 0;
			mDefaultTexture->pixelData[indexBpp + 1] = isWhite ? 255 : 0;
			mDefaultTexture->pixelData[indexBpp + 2] = 255;
			mDefaultTexture->pixelData[indexBpp + 3] = 255;
		}
	}
	mDefaultTexture->SetState(ResourceState::CPU_READY);

	// White Texture — 1×1 (1,1,1,1). Neutral identity for multiplicative slots
	// (specular strength, shininess, AO) — multiplying by 1 has no effect.
	mWhiteTexture = NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE);
	mWhiteTexture->SetUID(INVALID_ID - 2);
	mWhiteTexture->SetName("WhiteTexture");
	mWhiteTexture->width        = 1;
	mWhiteTexture->height       = 1;
	mWhiteTexture->channelCount = 4;
	mWhiteTexture->pixelData    = { 255, 255, 255, 255 };
	mWhiteTexture->SetState(ResourceState::CPU_READY);

	// Black Texture — 1×1 (0,0,0,1). Neutral identity for additive slots
	// (emissive) — adding 0 has no effect.
	mBlackTexture = NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE);
	mBlackTexture->SetUID(INVALID_ID - 3);
	mBlackTexture->SetName("BlackTexture");
	mBlackTexture->width        = 1;
	mBlackTexture->height       = 1;
	mBlackTexture->channelCount = 4;
	mBlackTexture->pixelData    = { 0, 0, 0, 255 };
	mBlackTexture->SetState(ResourceState::CPU_READY);

	// Flat Normal Texture — 1×1 tangent-space flat normal (128,128,255,255).
	// Decoded as (0,0,1) in [-1,1], giving the unperturbed geometry normal after
	// TBN multiplication. Using white as a fallback would produce a 45° tilt.
	mFlatNormalTexture = NOUS_NEW<ResourceTexture>(MemoryTag::RESOURCE_TEXTURE);
	mFlatNormalTexture->SetUID(INVALID_ID - 4);
	mFlatNormalTexture->SetName("FlatNormalTexture");
	mFlatNormalTexture->width        = 1;
	mFlatNormalTexture->height       = 1;
	mFlatNormalTexture->channelCount = 4;
	mFlatNormalTexture->pixelData    = { 128, 128, 255, 255 };
	mFlatNormalTexture->SetState(ResourceState::CPU_READY);
}

void ModuleResourceManager::CreateBuiltinMaterial()
{
	using enum UniformValueType;

	mDefaultMaterial = NOUS_NEW<ResourceMaterial>(MemoryTag::RESOURCE_MATERIAL);
	mDefaultMaterial->SetName("DefaultMaterial");
	mDefaultMaterial->uniformValues["diffuseColor"]      = { Vec4,  glm::vec4(1.0f) };
	mDefaultMaterial->uniformValues["emissiveColor"]     = { Vec4,  glm::vec4(1.0f) };
	mDefaultMaterial->uniformValues["aoIntensity"]       = { Float, glm::vec4(1.0f) };
	mDefaultMaterial->uniformValues["normalStrength"]    = { Float, glm::vec4(1.0f) };
	mDefaultMaterial->uniformValues["specularIntensity"] = { Float, glm::vec4(1.0f) };
	mDefaultMaterial->uniformValues["shininessScale"]    = { Float, glm::vec4(1.0f) };
	mDefaultMaterial->textureMaps["diffuseSampler"].texture = mDefaultTexture;
	mDefaultMaterial->SetState(ResourceState::CPU_READY);
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
	if (event.type == EventType::DROP_FILE)
		ImportFile(event.ctx.c);
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

	const std::string relativePath  = NOUS_FileManager::GetRelativePath(path);
	const std::string fileDirectory = NOUS_FileManager::GetDirectory(path);
	const std::string fileName      = NOUS_FileManager::GetFilename(path);
	const std::string extension     = NOUS_FileManager::GetExtension(path);
	const ResourceType resourceType = Resource::GetTypeFromExtension(extension);

	if (resourceType == ResourceType::UNKNOWN)
		return false;

	if (fileDirectory.rfind("Assets\\", 0) == 0)
		return ImportFileFromAssets(relativePath, resourceType, fileName, extension);

	return ImportFileFromExternal(path, resourceType, fileName, extension);
}

bool ModuleResourceManager::ImportFileFromExternal(const std::string& path, ResourceType resourceType,
                                                   const std::string& fileName, const std::string& extension)
{
	const std::string newPath = Resource::GetAssetsDirectoryFromType(resourceType) + fileName + extension;
	if (!NOUS_FileManager::CopyFile(path, newPath))
	{
		NOUS_ERROR("Import File ERROR: CASE 0 --> Error while copying the file to Assets\\ directory.");
		return false;
	}
	return ImportFile(newPath);
}

bool ModuleResourceManager::ImportFileFromAssets(const std::string& relativePath, ResourceType resourceType,
                                                  const std::string& fileName, const std::string& extension) const
{
	const std::string metaFilePath = Resource::GetAssetsDirectoryFromType(resourceType) + fileName + extension + ".meta";

	if (!NOUS_FileManager::Exists(metaFilePath))
		return ImportCase1_NewAsset(relativePath, metaFilePath, resourceType, fileName, extension);

	MetaFileData metaFileData;
	if (!ReadMetaFile(metaFilePath, metaFileData))
	{
		NOUS_ERROR("Import File ERROR: CASE 2,3 --> Error reading meta file: %s", metaFilePath.c_str());
		return false;
	}

	if (!NOUS_FileManager::Exists(metaFileData.libraryPath))
		return ImportCase2_MissingLibrary(metaFileData);

	return ImportCase3_TimestampCheck(metaFileData);
}

bool ModuleResourceManager::ImportCase1_NewAsset(const std::string_view relativePath, const std::string& metaFilePath,
                                                  ResourceType resourceType, const std::string_view fileName,
                                                  std::string_view extension) const
{
	const auto resourceUID           = static_cast<uint32>(Random::Generate());
	const std::string libExtension   = Resource::GetLibraryExtensionFromType(resourceType);
	std::string libraryPath          = std::format("{}{}", Resource::GetLibraryDirectoryFromType(resourceType), resourceUID);
	if (!libExtension.empty())
		libraryPath += "." + libExtension;

	MetaFileData metaFileData;
	metaFileData.name         = fileName;
	metaFileData.uid          = resourceUID;
	metaFileData.resourceType = resourceType;
	metaFileData.assetsPath   = relativePath;
	metaFileData.libraryPath  = libraryPath;

	if (!CreateMetaFile(metaFilePath, metaFileData))
	{
		NOUS_ERROR("Import File ERROR: CASE 1 --> Error creating meta file: %s", metaFilePath.c_str());
		return false;
	}

	mImporterManager->Import(metaFileData.resourceType, metaFileData);
	return true;
}

bool ModuleResourceManager::ImportCase2_MissingLibrary(const MetaFileData& metaFileData) const
{
	mImporterManager->Import(metaFileData.resourceType, metaFileData);
	return true;
}

bool ModuleResourceManager::ImportCase3_TimestampCheck(const MetaFileData& metaFileData) const
{
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
	return true;
}

std::unordered_map<uint32, Resource*> ModuleResourceManager::GetResourcesMap() const
{
	std::scoped_lock lock(resourcesMutex);
	return resources;
}

bool ModuleResourceManager::CreateMetaFile(const std::string& metaFilePath, const MetaFileData& inFileData)
{
	JsonFile metaFile;

	metaFile.AppendValue("Name", inFileData.name);
	metaFile.AppendValue("UID", static_cast<double>(inFileData.uid));
	metaFile.AppendValue("Resource Type", std::to_underlying(inFileData.resourceType));
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
	std::string r_fileName;
	std::string r_assetsPath;
	std::string r_libraryPath;

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
	outFileData.uid = static_cast<uint32>(r_resourceUID); // Casting double to UID
	outFileData.resourceType = static_cast<ResourceType>(r_resourceType); // Casting int to ResourceType
	outFileData.assetsPath = r_assetsPath;
	outFileData.libraryPath = r_libraryPath;

	return true;
}


Resource* ModuleResourceManager::InstantiateResource(ResourceType type)
{
    const auto it = k_ResourceFactories.find(type);
    if (it == k_ResourceFactories.end()) return nullptr;
    return it->second.create();
}

void ModuleResourceManager::DeleteResource(Resource*& resource)
{
	const uint32 uid = resource->GetUID();

	// Remove any sub-resource map entry that points to this UID.
	// Must hold resourcesMutex — RequestOrCreateSubMeshResource reads/writes this
	// map under the same lock from worker threads.
	{
		std::scoped_lock lock(resourcesMutex);
		for (auto it = m_submeshUIDMap.begin(); it != m_submeshUIDMap.end(); )
		{
			if (it->second == uid)
				it = m_submeshUIDMap.erase(it);
			else
				++it;
		}
	}

	const auto it = k_ResourceFactories.find(resource->GetType());
	if (it != k_ResourceFactories.end())
		it->second.destroy(resource);

	// NOTE: resources.erase(uid) is intentionally NOT here.
	// EvictResource removes the entry under resourcesMutex before calling DeleteResource,
	// so the map is clean before we reach this point.
}

bool ModuleResourceManager::ResourceExists(uint32 uid) const
{
	std::scoped_lock lock(resourcesMutex);
	return resources.contains(uid);
}

bool ModuleResourceManager::ClaimSlot(uint32 uid)
{
	std::scoped_lock lock(resourcesMutex);
	if (resources.contains(uid)) return false;
	resources[uid] = nullptr; // placeholder: marks slot as "in progress"
	return true;
}

Resource* ModuleResourceManager::SpinWaitForSlot(uint32 uid)
{
	Resource* resource = nullptr;
	while (true)
	{
		{
			std::scoped_lock lock(resourcesMutex);
			const auto it = resources.find(uid);
			if (it == resources.end())
				return nullptr; // evicted before it resolved; caller should retry
			resource = it->second;
			if (resource != nullptr)
			{
				resource->IncreaseReferenceCount(); // under lock — closes the eviction race
				break;
			}
		}
		NOUS_Multithreading::NOUS_Thread::SleepMS(1);
	}
	resource->Validate();
	return resource;
}

Resource* ModuleResourceManager::LoadResourceIntoSlot(uint32 uid, ResourceType type,
    const std::string& name, const std::string& assetsPath, const std::string& libraryPath)
{
	Resource* resource = InstantiateResource(type);
	if (!resource)
	{
		std::scoped_lock lock(resourcesMutex);
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
		NOUS_ERROR("LoadResourceIntoSlot: failed to deserialize '%s' from '%s'", name.c_str(), libraryPath.c_str());
		DeleteResource(resource);
		std::scoped_lock lock(resourcesMutex);
		resources.erase(uid);
		return nullptr;
	}

	resource->SetState(ResourceState::CPU_READY);
	{ std::scoped_lock lock(resourcesMutex);        resources[uid] = resource; }
	{ std::scoped_lock lock(m_pendingUploadsMutex); m_pendingUploads.emplace_back(type, resource); }
	resource->IncreaseReferenceCount();
	resource->Validate();
	return resource;
}

ResourceMesh* ModuleResourceManager::BuildAndRegisterSubMesh(
    std::pair<uint32, int32_t> cacheKey,
    const std::string& libraryPath,
    int32_t submeshIndex,
    const std::string& assetsPath)
{
	const auto hierarchy = ImporterMesh::LoadHierarchy(libraryPath);
	if (submeshIndex < 0 || submeshIndex >= static_cast<int32_t>(hierarchy.size()))
	{
		NOUS_ERROR("BuildAndRegisterSubMesh: index %d out of range (count=%zu) for '%s'",
		    submeshIndex, hierarchy.size(), libraryPath.c_str());
		return nullptr;
	}

	const SubMeshData& sub = hierarchy[static_cast<size_t>(submeshIndex)];

	auto* mesh = NOUS_NEW<ResourceMesh>(MemoryTag::RESOURCE_MESH);
	mesh->SetName(sub.name);
	mesh->SetType(ResourceType::MESH);
	mesh->SetLibraryPath(libraryPath);
	if (!assetsPath.empty())
		mesh->SetAssetsPath(assetsPath);

	mesh->vertices = sub.vertices;
	mesh->indices.assign(sub.indices.begin(), sub.indices.end());
	mesh->SetState(ResourceState::CPU_READY);

	uint32 uid;
	{
		std::lock_guard lock(resourcesMutex);
		do { uid = static_cast<uint32>(Random::Generate()); }
		while (uid == 0 || resources.contains(uid));
		resources[uid]             = mesh;
		m_submeshUIDMap[cacheKey]  = uid;
	}
	{
		std::lock_guard lock(m_pendingUploadsMutex);
		m_pendingUploads.emplace_back(ResourceType::MESH, mesh);
	}

	mesh->SetUID(uid);
	mesh->IncreaseReferenceCount();
	mesh->Validate();
	return mesh;
}

Resource* ModuleResourceManager::CreateResource(const std::string& assetsPath)
{
	MetaFileData metaFileData;
	if (!ReadMetaFile(assetsPath + ".meta", metaFileData))
	{
		NOUS_ERROR("CreateResource: failed to read meta for '%s'", assetsPath.c_str());
		return nullptr;
	}

	if (ClaimSlot(metaFileData.uid))
		return LoadResourceIntoSlot(metaFileData.uid, metaFileData.resourceType,
		    metaFileData.name, metaFileData.assetsPath, metaFileData.libraryPath);

	return SpinWaitForSlot(metaFileData.uid);
}

Resource* ModuleResourceManager::CreateResourceFromLibrary(const uint32 uid, ResourceType type,
                                                            const std::string& name,
                                                            const std::string& assetsPath,
                                                            const std::string& libraryPath)
{
	if (ClaimSlot(uid))
		return LoadResourceIntoSlot(uid, type, name, assetsPath, libraryPath);

	return SpinWaitForSlot(uid);
}

ResourceMesh* ModuleResourceManager::RequestOrCreateSubMeshResourceFromLibrary(
    const std::string& libraryPath, int32_t submeshIndex, const std::string& assetsPath)
{
	// Use hash(libraryPath) as a stable synthetic base UID for dedup.
	const auto baseUID = static_cast<uint32>(std::hash<std::string>{}(libraryPath) & 0xFFFFFFFF);
	const auto key = std::make_pair(baseUID, submeshIndex);

	{
		std::scoped_lock lock(resourcesMutex);
		if (const auto mapIt = m_submeshUIDMap.find(key); mapIt != m_submeshUIDMap.end())
		{
			if (const auto resIt = resources.find(mapIt->second); resIt != resources.end() && resIt->second)
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

	return BuildAndRegisterSubMesh(key, libraryPath, submeshIndex, assetsPath);
}

bool ModuleResourceManager::UnloadResource(uint32 uid)
{
	Resource* resource = nullptr;
	{
		std::lock_guard lock(resourcesMutex);
		const auto it = resources.find(uid);
		if (it == resources.end() || !it->second) return false; // not found or still loading (placeholder)
		resource = it->second;
		resource->DecreaseReferenceCount();
	}

	// Defer GPU Release + CPU Evict to the Renderer's PreUpdate so we never
	// destroy GPU resources mid-frame (between command recording and vkQueueSubmit).
	if (resource->GetReferenceCount() == 0)
	{
		std::lock_guard lock(m_pendingReleasesMutex);
		m_pendingReleases.emplace_back(resource->GetType(), resource);
	}

	return true;
}


void ModuleResourceManager::DestroyBuiltinTexture(ResourceTexture*& tex, IGPUResourceFactory* gpu) const
{
    if (!tex) return;
    if (tex->GetState() == ResourceState::GPU_READY)
        gpu->DestroyTexture(tex);
    NOUS_DELETE(tex, MemoryTag::RESOURCE_TEXTURE);
    tex = nullptr;
}

void ModuleResourceManager::ClearResources(IGPUResourceFactory* gpu)
{
    // Destroy shaders FIRST — descriptor sets reference texture image views;
    // freeing textures first would leave dangling view references in the sets.
    // VUID-vkDestroyImageView-01026
    for (auto& res : resources | std::views::values)
    {
        if (!res || res->GetType() != ResourceType::SHADER) continue;
        if (res->GetState() == ResourceState::GPU_READY)
            gpu->DestroyShader(down_cast<ResourceShader*>(res));
        mImporterManager->Evict(ResourceType::SHADER, res);
        NOUS_DELETE(res, MemoryTag::RESOURCE_SHADER);
    }

    // Materials next — destroy GPU handle, then clear the texture pointer directly
    // (do NOT call UnloadResource here; the texture will be destroyed in the next pass).
    for (auto& res : resources | std::views::values)
    {
        if (!res || res->GetType() != ResourceType::MATERIAL) continue;
        if (res->GetState() == ResourceState::GPU_READY)
            gpu->DestroyMaterial(down_cast<ResourceMaterial*>(res));
        for (auto* mat = down_cast<ResourceMaterial*>(res); auto& map : mat->textureMaps | std::views::values)
            map.texture = nullptr;
        NOUS_DELETE(res, MemoryTag::RESOURCE_MATERIAL);
    }

    // Textures
    for (auto& res : resources | std::views::values)
    {
        if (!res || res->GetType() != ResourceType::TEXTURE) continue;
        if (res->GetState() == ResourceState::GPU_READY)
            gpu->DestroyTexture(down_cast<ResourceTexture*>(res));
        mImporterManager->Evict(ResourceType::TEXTURE, res);
        NOUS_DELETE(res, MemoryTag::RESOURCE_TEXTURE);
    }

    // Meshes
    for (auto& res : resources | std::views::values)
    {
        if (!res || res->GetType() != ResourceType::MESH) continue;
        if (res->GetState() == ResourceState::GPU_READY)
            gpu->DestroyGeometry(down_cast<ResourceMesh*>(res));
        mImporterManager->Evict(ResourceType::MESH, res);
        NOUS_DELETE(res, MemoryTag::RESOURCE_MESH);
    }

    resources.clear();
    m_submeshUIDMap.clear();

    DestroyBuiltinTexture(mDefaultTexture,    gpu);
    DestroyBuiltinTexture(mWhiteTexture,      gpu);
    DestroyBuiltinTexture(mBlackTexture,      gpu);
    DestroyBuiltinTexture(mFlatNormalTexture, gpu);

    if (mDefaultMaterial)
    {
        if (mDefaultMaterial->GetState() == ResourceState::GPU_READY)
            gpu->DestroyMaterial(mDefaultMaterial);
        NOUS_DELETE(mDefaultMaterial, MemoryTag::RESOURCE_MATERIAL);
        mDefaultMaterial = nullptr;
    }

    // Discard any stale queue entries — all resources are destroyed above.
    { std::lock_guard lock(m_pendingUploadsMutex);  m_pendingUploads.clear(); }
    { std::lock_guard lock(m_pendingReleasesMutex); m_pendingReleases.clear(); }
}

std::vector<std::pair<ResourceType, Resource*>> ModuleResourceManager::TakePendingUploads()
{
    std::vector<std::pair<ResourceType, Resource*>> result;
    std::scoped_lock lock(m_pendingUploadsMutex);
    std::swap(result, m_pendingUploads);
    return result;
}

std::vector<std::pair<ResourceType, Resource*>> ModuleResourceManager::TakePendingReleases()
{
    std::vector<std::pair<ResourceType, Resource*>> result;
    std::scoped_lock lock(m_pendingReleasesMutex);
    std::swap(result, m_pendingReleases);
    return result;
}

bool ModuleResourceManager::EvictResource(ResourceType type, Resource* resource)
{
    // Final refcount check + map erasure under the same lock so that a concurrent
    // CreateResource thread cannot bump the refcount between our check and the delete.
    {
        std::lock_guard lock(resourcesMutex);
        if (resource->GetReferenceCount() > 0)
        {
            // Re-acquired since TakePendingReleases. ImporterManager::Release already
            // freed the GPU resources, so re-queue for upload to restore GPU state.
            std::lock_guard uploadLock(m_pendingUploadsMutex);
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

Resource* ModuleResourceManager::GetLoadedResource(const uint32 uid)
{
    std::lock_guard lock(resourcesMutex);
    const auto it = resources.find(uid);
    if (it == resources.end() || it->second == nullptr)
        return nullptr;
    return it->second;
}

IImporterManager* ModuleResourceManager::GetImporterManager() const
{
    return mImporterManager;
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
        std::lock_guard lock(resourcesMutex);
        if (const auto mapIt = m_submeshUIDMap.find(key); mapIt != m_submeshUIDMap.end())
        {
            if (const auto resIt = resources.find(mapIt->second); resIt != resources.end() && resIt->second)
            {
                resIt->second->IncreaseReferenceCount();
                return down_cast<ResourceMesh*>(resIt->second);
            }
            // Stale entry (resource was destroyed) — remove and recreate below.
            m_submeshUIDMap.erase(mapIt);
        }
    }

    return BuildAndRegisterSubMesh(key, metaData.libraryPath, submeshIndex, assetsPath);
}

struct ModuleResourceManager::MeshRequest
{
    std::string assetPath;
    std::string libraryPath;
    uint32      uid          = 0;
    int32_t     submeshIndex = -1;
};

Resource* ModuleResourceManager::LoadMeshRequest(const MeshRequest& req)
{
    if (req.submeshIndex >= 0)
    {
        if (!req.libraryPath.empty())
            return RequestOrCreateSubMeshResourceFromLibrary(req.libraryPath, req.submeshIndex, req.assetPath);
        return RequestOrCreateSubMeshResource(req.assetPath, req.submeshIndex);
    }

    if (!req.libraryPath.empty() && req.uid != 0)
        return CreateResourceFromLibrary(req.uid, ResourceType::MESH,
            std::filesystem::path(req.assetPath).filename().string(),
            req.assetPath, req.libraryPath);

    return CreateResource(req.assetPath);
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

    JSON_Object const* rootObj    = json_value_get_object(root);
    JSON_Array const*  gameObjects = json_object_get_array(rootObj, "GameObjects");

    if (!gameObjects)
    {
        json_value_free(root);
        return futures;
    }

    // Collect every unique CMesh resource request in the scene file.
    // Key: (assetPath, submeshIndex) — same deduplication the ResourceManager uses internally.

    std::map<std::pair<std::string, int32_t>, MeshRequest> uniqueRequests;

    const size_t goCount = json_array_get_count(gameObjects);
    for (size_t i = 0; i < goCount; ++i)
    {
        JSON_Object const* goObj     = json_array_get_object(gameObjects, i);
        JSON_Array const*  comps     = json_object_get_array(goObj, "components");
        if (!comps) continue;

        const size_t compCount = json_array_get_count(comps);
        for (size_t j = 0; j < compCount; ++j)
        {
            JSON_Object const* compObj = json_array_get_object(comps, j);

            if (const char* type = json_object_get_string(compObj, "type");
            	!type || std::strcmp(type, "CMesh") != 0)
            	continue;

            const char* assetPath = json_object_get_string(compObj, "assetPath");
            if (!assetPath || assetPath[0] == '\0') continue;

            const char*        libRaw   = json_object_get_string(compObj, "libraryPath");
            const JSON_Value*  uidVal   = json_object_get_value(compObj, "resourceUID");
            const JSON_Value*  subIdxV  = json_object_get_value(compObj, "submeshIndex");

            MeshRequest req;
            req.assetPath    = assetPath;
            req.libraryPath  = libRaw ? libRaw : "";
            req.uid          = uidVal   ? static_cast<uint32>(json_value_get_number(uidVal))   : 0;
            req.submeshIndex = subIdxV  ? static_cast<int32_t>(json_value_get_number(subIdxV)) : -1;

            uniqueRequests[{req.assetPath, req.submeshIndex}] = std::move(req);
        }
    }

    json_value_free(root);

    // Submit one parallel Deserialize job per unique resource.
    futures.reserve(uniqueRequests.size());

    for (const auto& req : uniqueRequests | std::views::values)
    {
        auto prom = std::make_shared<std::promise<void>>();
        futures.push_back(prom->get_future());

        jobSystem->SubmitJob([this, req, prom]
        {
            // Release the preload's reference. The resource stays in the map so
            // CMesh::Deserialize() hits the fast path, but the preload does not
            // hold an extra ref that would prevent eviction.
            if (Resource* res = LoadMeshRequest(req)) res->DecreaseReferenceCount();
            prom->set_value();
        }, "Preload: " + req.assetPath);
    }

    NOUS_INFO_C(CURRENT_CHANNEL,
        "PreloadSceneResourcesAsync: submitted %zu parallel jobs for '%s'",
        uniqueRequests.size(), sceneFilePath.c_str());

    return futures;
}