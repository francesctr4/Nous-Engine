#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Renderer/IGPUResourceFactory.h"
#include "Engine/Modules/ModuleInput/include/ModuleInput.h"
#include <filesystem>
#include <algorithm>
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Utils/Serialization/Random/Random.h"
#include "Engine/Utils/Serialization/JsonFile/JsonFile.h"
#include "Engine/Systems/ResourceManager/Resource/MetaFileData.inl"

#include "Engine/Systems/ResourceManager/Importer/IImporterManager.h"
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
    : Module(eventSystem, jobSystem)
    , mImporterManager(importerManager)
    , m_importPipeline(importerManager)
{
	eventSystem->Subscribe(EventType::DROP_FILE, this);
}

ModuleResourceManager::~ModuleResourceManager() = default;

bool ModuleResourceManager::Awake()
{
	mImporterManager->Init(this);

	// Always ensure directories exist — idempotent, safe to call in any mode.
	m_importPipeline.EnsureLibraryDirectories();

	return true;
}

void ModuleResourceManager::ScanAndImportAssets()
{
	m_importPipeline.ScanAndImportAssets();
}

bool ModuleResourceManager::ImportDirectory(const std::string& directory)
{
	return m_importPipeline.ImportDirectory(directory);
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

bool ModuleResourceManager::ImportFile(const std::string& path)
{
	return m_importPipeline.ImportFile(path);
}

std::unordered_map<uint32, Resource*> ModuleResourceManager::GetResourcesMap() const
{
	std::scoped_lock lock(resourcesMutex);
	return resources;
}


Resource* ModuleResourceManager::InstantiateResource(const ResourceType type)
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

	if (const auto it = k_ResourceFactories.find(resource->GetType());
		it != k_ResourceFactories.end())
		it->second.destroy(resource);

	// NOTE: resources.erase(uid) is intentionally NOT here.
	// EvictResource removes the entry under resourcesMutex before calling DeleteResource,
	// so the map is clean before we reach this point.
}

bool ModuleResourceManager::ResourceExists(const uint32 uid) const
{
	std::scoped_lock lock(resourcesMutex);
	return resources.contains(uid);
}

bool ModuleResourceManager::ClaimSlot(const uint32 uid)
{
	std::scoped_lock lock(resourcesMutex);
	if (resources.contains(uid)) return false;
	resources[uid] = nullptr; // placeholder: marks slot as "in progress"
	return true;
}

Resource* ModuleResourceManager::SpinWaitForSlot(const uint32 uid)
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

Resource* ModuleResourceManager::LoadResourceIntoSlot(const uint32 uid, ResourceType type,
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
    const std::pair<uint32, int32_t> cacheKey,
    const std::string& libraryPath,
    const int32_t submeshIndex,
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
	if (!ResourceImportPipeline::GetAssetMetaData(assetsPath, metaFileData))
	{
		NOUS_ERROR("CreateResource: failed to read meta for '%s'", assetsPath.c_str());
		return nullptr;
	}

	if (ClaimSlot(metaFileData.uid))
		return LoadResourceIntoSlot(metaFileData.uid, metaFileData.resourceType,
		    metaFileData.name, metaFileData.assetsPath, metaFileData.libraryPath);

	return SpinWaitForSlot(metaFileData.uid);
}

Resource* ModuleResourceManager::CreateResourceFromLibrary(const uint32 uid, const ResourceType type,
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

bool ModuleResourceManager::UnloadResource(const uint32 uid)
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


void ModuleResourceManager::DestroyBuiltinTexture(ResourceTexture*& tex, IGPUResourceFactory* gpu)
{
    if (!tex) return;
    if (tex->GetState() == ResourceState::GPU_READY)
        gpu->DestroyTexture(tex);
    NOUS_DELETE(tex, MemoryTag::RESOURCE_TEXTURE);
    tex = nullptr;
}

void ModuleResourceManager::ClearResources(IGPUResourceFactory* gpu)
{
    // Common pattern for types that don't need special pre-destruction work:
    // filter by type → GPU destroy (if ready) → CPU Evict → DELETE.
    // Defined as a lambda so its body is a separate complexity unit.
    auto destroyByType = [&](const ResourceType type, const MemoryTag tag, auto gpuDestroy)
    {
        for (auto& res : resources | std::views::values)
        {
            if (!res || res->GetType() != type) continue;
            if (res->GetState() == ResourceState::GPU_READY)
                gpuDestroy(res);
            mImporterManager->Evict(type, res);
            NOUS_DELETE(res, tag);
        }
    };

    // Shaders first — descriptor sets reference texture image views;
    // freeing textures first would leave dangling view references in the sets.
    // VUID-vkDestroyImageView-01026
    destroyByType(ResourceType::SHADER, MemoryTag::RESOURCE_SHADER,
        [&gpu](Resource* r) { gpu->DestroyShader(down_cast<ResourceShader*>(r)); });

    // Materials next — destroy GPU handle, then clear texture pointers directly
    // (do NOT call UnloadResource; textures are destroyed in the next pass).
    for (auto& res : resources | std::views::values)
    {
        if (!res || res->GetType() != ResourceType::MATERIAL) continue;
        auto* mat = down_cast<ResourceMaterial*>(res);
        if (mat->GetState() == ResourceState::GPU_READY)
            gpu->DestroyMaterial(mat);
        for (auto& map : mat->textureMaps | std::views::values)
            map.texture = nullptr;
        NOUS_DELETE(res, MemoryTag::RESOURCE_MATERIAL);
    }

    destroyByType(ResourceType::TEXTURE, MemoryTag::RESOURCE_TEXTURE,
        [&gpu](Resource* r) { gpu->DestroyTexture(down_cast<ResourceTexture*>(r)); });

    destroyByType(ResourceType::MESH, MemoryTag::RESOURCE_MESH,
        [&gpu](Resource* r) { gpu->DestroyGeometry(down_cast<ResourceMesh*>(r)); });

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


ResourceMesh* ModuleResourceManager::RequestOrCreateSubMeshResource(const std::string& assetsPath,
                                                                      int32_t submeshIndex)
{
    // Read meta to resolve the base UID and library path.
    MetaFileData metaData;
    if (!ResourceImportPipeline::GetAssetMetaData(assetsPath, metaData))
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

// Walks the GameObjects array and collects every unique CMesh request.
// Key: (assetPath, submeshIndex) — mirrors ResourceManager's internal deduplication.
void ModuleResourceManager::CollectMeshRequestsFromScene(
    JSON_Array const* gameObjects,
    std::map<std::pair<std::string, int32_t>, MeshRequest>& outRequests)
{
    const size_t goCount = json_array_get_count(gameObjects);
    for (size_t i = 0; i < goCount; ++i)
    {
        JSON_Object const* goObj = json_array_get_object(gameObjects, i);
        JSON_Array const*  comps = json_object_get_array(goObj, "components");
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

            const char*       libRaw  = json_object_get_string(compObj, "libraryPath");
            const JSON_Value* uidVal  = json_object_get_value(compObj, "resourceUID");
            const JSON_Value* subIdxV = json_object_get_value(compObj, "submeshIndex");

            MeshRequest req;
            req.assetPath    = assetPath;
            req.libraryPath  = libRaw  ? libRaw : "";
            req.uid          = uidVal  ? static_cast<uint32>(json_value_get_number(uidVal))   : 0;
            req.submeshIndex = subIdxV ? static_cast<int32_t>(json_value_get_number(subIdxV)) : -1;

            outRequests[{req.assetPath, req.submeshIndex}] = std::move(req);
        }
    }
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

    JSON_Object const* rootObj     = json_value_get_object(root);
    JSON_Array const*  gameObjects = json_object_get_array(rootObj, "GameObjects");

    if (!gameObjects)
    {
        json_value_free(root);
        return futures;
    }

    std::map<std::pair<std::string, int32_t>, MeshRequest> uniqueRequests;
    CollectMeshRequestsFromScene(gameObjects, uniqueRequests);
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