#include <ModuleRenderer3D/ModuleRenderer3D.h>
#include <ModuleRenderer3D/SkinningPairing.h>

#include "RenderPacketPolicy.h"
#include <EngineCore/InvalidID.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
// glm::rotation lives in a gtx/ header. Repo convention: the define goes
// immediately above the include and ONLY in a .cpp, never a header.
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <cmath>
#include <ModuleWindow/ModuleWindow.h>
#include <ModuleCamera3D/ModuleCamera3D.h>
#include <ModuleScene/ModuleScene.h>
#include <ResourceManager/Core/IResourceGpuSync.h>
#include <ResourceManager/Core/IResourceLoader.h>
#include <Renderer/iRenderResourceProvider.h>

#include <RendererFrontend/RendererFrontend.h>

#include <ModuleScene/SceneRenderData.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CMesh/CMesh.h>
#include <ECS/Component/Types/CMaterial/CMaterial.h>
#include <ECS/Component/Types/CVideoPlayer/CVideoPlayer.h>
#include <ECS/Component/Types/CTransform/CTransform.h>
#include <ECS/Component/Types/CCamera/CCamera.h>
#include <ECS/Component/Types/CLight/CLight.h>
#include <ECS/Component/Types/CAnimator/CAnimator.h>
#include <ECS/ECSInternalComponents.h>
#include <ResourceManager/Types/ResourceSkeleton/ResourceSkeleton.h>
#include <AnimationSystem/Bounds.h>
#include <AnimationSystem/Palette.h>
#include <Utils/Math/Vertex.inl>

#include <MemoryManager/MemoryManager.h>
#include <EventSystem/EventSystem.h>
#include <Logger/LogChannel.h>
#include <Logger/Logger.h>
#include <ResourceManager/Core/AssetPaths.h>
#include <ResourceManager/Core/ResourceBase.h>
#include <ResourceManager/Types/ResourceTexture/ResourceTexture.h>
#include <ResourceManager/Core/IImporterManager.h>
#include <FileSystem/FileSystem.h>

#include <filesystem>
#include <Utils/Serialization/JsonFile.h>
#include <CameraSystem/Camera.h>
#include <ResourceManager/Types/ResourceMaterial/ResourceMaterial.h>
#include <ResourceManager/Types/ResourceMesh/ResourceMesh.h>
#include <Utils/Math/FrustumCulling.h>


#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MODULE_RENDERER3D;

void ApplySkinningToGeometry(const entt::registry& registry, const entt::entity entity,
                             const ResourceMesh& mesh, GeometryRenderData& data)
{
	if (!mesh.hasSkinning)
		return;

	const auto* hierarchy = registry.try_get<CHierarchy>(entity);
	if (!hierarchy || hierarchy->parent == entt::null)
		return;

	// Both halves are load-bearing: a rigged mesh with no animator must render
	// statically, and an animator that has not bound yet publishes an empty palette
	// which must not be mistaken for a pose.
	const auto* animator = registry.try_get<CAnimator>(hierarchy->parent);
	if (!animator || animator->GetPalette().empty())
		return;

	data.palette = &animator->GetPalette();

	// The palette already maps mesh space -> animated MODEL space, so the only
	// transform left to apply is where the rig sits in the world -- the ANIMATOR
	// ROOT's world matrix, not this child's. Using the child's composes its FBX node
	// transform a second time.
	if (const auto* rootTransform = registry.try_get<CTransform>(hierarchy->parent))
		data.model = rootTransform->worldMatrix;
}

ModuleRenderer3D::ModuleRenderer3D(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem,
	ModuleWindow* moduleWindow, ModuleCamera3D* moduleCamera,
	IResourceGpuSync* resourceGpuSync, IResourceLoader* resourceLoader,
	IRenderResourceProvider* resourceProvider, ModuleScene* moduleScene) :
		Module(eventSystem, jobSystem), mModuleWindow(moduleWindow), mModuleCamera3D(moduleCamera),
		mModuleScene(moduleScene),
		mResourceGpuSync(resourceGpuSync),
		mResourceLoader(resourceLoader),
		mResourceProvider(resourceProvider)
{
	eventSystem->Subscribe(EventType::WINDOW_RESIZED, this);
	eventSystem->Subscribe(EventType::WINDOW_MINIMIZED, this);

	mRendererFrontend = NOUS_NEW<RendererFrontend>(MemoryTag::RENDERER);
}

ModuleRenderer3D::~ModuleRenderer3D()
{
	NOUS_DELETE(mRendererFrontend, MemoryTag::RENDERER);
}

void ModuleRenderer3D::UpdateShaderWatcherPath(const std::string& oldPath, const std::string& newPath)
{
	m_shaderWatcher.Unwatch(oldPath);
	WatchShaderFile(newPath);
}

void ModuleRenderer3D::WatchShaderFile(const std::string& path)
{
	if (m_renderMode != RenderMode::EDITOR) return;

	const std::string normalizedPath = std::filesystem::path(path).generic_string();
	m_shaderWatcher.Watch(normalizedPath, [this, normalizedPath](const std::string&)
	{
		mRendererFrontend->ReloadShaderByPath(normalizedPath);
	});
}

void ModuleRenderer3D::SetRenderMode(RenderMode mode) noexcept
{
	m_renderMode = mode;
	// Stored in RendererFrontend; forwarded to VulkanContext inside Initialize().
	mRendererFrontend->SetRenderMode(mode);
}

bool ModuleRenderer3D::Awake()
{
	mRendererFrontend->SetBackendType(RendererBackendType::VULKAN);

	mRendererFrontend->InjectDependencies(mModuleWindow, eventSystem, JobSystem, mResourceProvider);

	if (!mRendererFrontend->Initialize(mRendererFrontend->GetBackendType()))
	{
		NOUS_FATAL_C(CURRENT_CHANNEL, "Failed to initialize renderer frontend with backend of type (%d). Aborting application.",
				   static_cast<int>(mRendererFrontend->GetBackendType()));
		return false;
	}

	return true;
}

bool ModuleRenderer3D::Start()
{
	// ------------------------------ SHADERS ------------------------------ //
	// Library/ is fully populated by the time Start() runs (ResourceManager::Awake
	// + ScanAndImportAssets already completed), so CreateResource is safe here.

	if (m_renderMode == RenderMode::GAME)
	{
		// GAME mode: load built-in shaders from the manifest written by the editor.
		// No .meta files required — UIDs and library paths are stored in Library/.
		LoadShadersFromManifest();
	}
	else
	{
		// EDITOR mode: load via asset path (reads .meta to resolve UID).
		// The shader manifest is written by ImportPipeline at the end of
		// ScanAndImportAssets, so no manifest write is needed here.
		mResourceLoader->CreateResource("Assets/Shaders/BuiltIn.MaterialShader.glsl");
		mResourceLoader->CreateResource("Assets/Shaders/BuiltIn.BackgroundShader.glsl");
		mResourceLoader->CreateResource("Assets/Shaders/BuiltIn.PickShader.glsl");
		mResourceLoader->CreateResource("Assets/Shaders/BuiltIn.OutlineShader.glsl");
		mResourceLoader->CreateResource("Assets/Shaders/BuiltIn.GridShader.glsl");
		mResourceLoader->CreateResource("Assets/Shaders/BuiltIn.BoundingBoxShader.glsl");
	}

	// Drain the initial upload queue — includes the default texture/material (queued
	// by ResourceManager::Start) and all shaders loaded above.  All must be
	// GPU_READY before the first frame renders.
	//
	// Materials depend on shader instance pools, but shaders are queued AFTER the
	// default material. Collect failed materials and retry after the full drain.
	IImporterManager* importer = mResourceGpuSync->GetImporterManager();
	std::vector<std::pair<ResourceType, ResourceBase*>> deferredUploads;
	for (auto& [type, resource] : mResourceGpuSync->TakePendingUploads())
	{
		if (!importer->Upload(type, resource, mRendererFrontend))
			deferredUploads.emplace_back(type, resource);
		else
			resource->SetState(ResourceState::GPU_READY);
	}

	// Retry deferred uploads — shaders (and their instance pools) are now ready.
	for (auto& [type, resource] : deferredUploads)
	{
		if (!importer->Upload(type, resource, mRendererFrontend))
			NOUS_ERROR("ModuleRenderer3D::Start() — failed to upload resource '%s'.", resource->GetName().c_str());
		resource->SetState(ResourceState::GPU_READY);
	}

	// Give all built-in fallback textures stable generation values so the descriptor
	// lazy-write cache (WriteInstanceSampler) can skip redundant writes. Without this,
	// generation stays UINT32_MAX after every write (treated as "never written"),
	// causing vkUpdateDescriptorSets to fire every draw call — and in EDITOR mode the
	// GAME pass update then hits a descriptor already recorded in the SCENE command
	// buffer, triggering a validation error. The unique identity key is GetUID(),
	// assigned in ModuleResourceManager::Start() (INVALID_ID - 1..4).
	ResourceTexture* defaultTex = mResourceProvider->GetDefaultTexture();
	if (defaultTex)
		defaultTex->generation = 0;
	ResourceTexture* whiteTex = mResourceProvider->GetWhiteTexture();
	if (whiteTex)
		whiteTex->generation = 0;
	ResourceTexture* blackTex = mResourceProvider->GetBlackTexture();
	if (blackTex)
		blackTex->generation = 0;
	ResourceTexture* flatNormalTex = mResourceProvider->GetFlatNormalTexture();
	if (flatNormalTex)
		flatNormalTex->generation = 0;

// Register all .glsl files in Assets/Shaders/ for hot reload (EDITOR only).
	// Changes are detected by Poll() in PreUpdate() and trigger a per-file reload.
	if (m_renderMode == RenderMode::EDITOR)
	{
		namespace fs = std::filesystem;
		std::error_code ec;
		int watchCount = 0;

		for (const auto& entry : fs::recursive_directory_iterator(nous::engine::asset_paths::k_AssetsDir, ec))
		{
			if (!entry.is_regular_file()) continue;
			if (entry.path().extension() != ".glsl") continue;

			// Normalize to forward slashes — must match the paths in ResourceManager.
			const std::string normalizedPath = entry.path().generic_string();
			m_shaderWatcher.Watch(normalizedPath, [this, normalizedPath](const std::string&)
			{
				mRendererFrontend->ReloadShaderByPath(normalizedPath);
			});
			++watchCount;
		}

		if (!ec)
			NOUS_INFO_C(CURRENT_CHANNEL, "[ShaderHotReload] Watching %d shader file(s) under Assets/.", watchCount);
		else
			NOUS_WARN_C(CURRENT_CHANNEL, "[ShaderHotReload] Could not scan Assets/ for shader watching: %s", ec.message().c_str());
	}

	return true;
}

void ModuleRenderer3D::FlushPendingAssetUploads()
{
	auto uploads = mResourceGpuSync->TakeReadyAssetUploads();
	if (uploads.empty()) return;

	// Drain all in-flight GPU frames before destroying any resources.
	// With triple-buffering, frames N-1 and N-2 may still reference the old
	// VkImageView/VkSampler/VkBuffer handles. Destroying without waiting causes
	// use-after-free validation errors and VK_ERROR_DEVICE_LOST on the second reload.
	mRendererFrontend->WaitForGPUIdle();

	IImporterManager* importer = mResourceGpuSync->GetImporterManager();
	for (auto& [uid, type] : uploads)
	{
		ResourceBase* resource = mResourceGpuSync->GetLoadedResource(uid);
		if (!resource) continue;

		// Save the generation BEFORE Deserialize — it resets texture generation to 0,
		// which can collide with the previous value and fool the lazy descriptor-write
		// guard into skipping the re-write after a hot-reload.
		const uint32_t preReloadGeneration = (type == ResourceType::TEXTURE)
		    ? static_cast<ResourceTexture*>(resource)->generation : 0;

		// Deserialize on the main thread so it never races with DrawGeometryBatched.
		// The worker only ran Import (library binary write); we apply the result here.
		if (!importer->Deserialize(type, resource->GetLibraryPath(), resource))
		{
			NOUS_ERROR("FlushPendingAssetUploads — Deserialize failed for '%s'; old asset kept.", resource->GetName().c_str());
			continue;
		}

		importer->Release(type, resource, mRendererFrontend);

		if (type == ResourceType::MATERIAL)
		{
			auto* mat = static_cast<ResourceMaterial*>(resource);
			mat->internalID     = INVALID_ID;
			mat->poolOwnerShader = nullptr;
		}

		if (!importer->Upload(type, resource, mRendererFrontend))
		{
			NOUS_ERROR("FlushPendingAssetUploads — failed to re-upload '%s'.", resource->GetName().c_str());
			continue;
		}

		if (type == ResourceType::TEXTURE)
		{
			// Set generation to preReloadGeneration + 1 instead of a plain ++.
			// Deserialize resets generation to 0, so ++ alone gives the same value
			// as the pre-reload generation and the lazy write guard skips the update.
			static_cast<ResourceTexture*>(resource)->generation = preReloadGeneration + 1;
		}
	}
}

UpdateStatus ModuleRenderer3D::PreUpdate(float dt)
{
#ifdef _PROFILING
	ZoneScopedN("ModuleRenderer3D::PreUpdate");
#endif
	// Re-upload assets that were reimported since last frame (hot reload).
	FlushPendingAssetUploads();

	{
#ifdef _PROFILING
		ZoneScopedN("FlushCompletedReloads");
#endif
		// Apply GPU swaps for compile jobs that completed since last frame (async path).
		// Must run before FlushPendingReloads and before DrawFrame — no renderpass open here.
		mRendererFrontend->FlushCompletedReloads();
	}

	// Upload CPU_READY resources before processing reslots so that a newly-imported
	// custom shader is GPU_READY when FlushPendingReslots calls CreateMaterial.
	// Without this ordering, a reslot that fires in the same frame the target shader
	// is first uploaded would see the shader as not-GPU_READY and fall back to vsBase's
	// instance pool — causing a NULL descriptor-set error on the first draw call.
	IImporterManager* importer = mResourceGpuSync->GetImporterManager();
	{
#ifdef _PROFILING
		ZoneScopedN("GPU Uploads");
#endif
		for (auto& [type, resource] : mResourceGpuSync->TakePendingUploads())
		{
			if (!importer->Upload(type, resource, mRendererFrontend))
				NOUS_ERROR("ModuleRenderer3D::PreUpdate() — failed to upload resource '%s'.", resource->GetName().c_str());
			resource->SetState(ResourceState::GPU_READY);
		}
	}

	// Process queued material shader changes (Inspector reslots). Must run after
	// FlushCompletedReloads (hot-reload GPU swaps) and TakePendingUploads (first-load
	// shader uploads) so the target shader is always GPU-ready when CreateMaterial runs.
	{
#ifdef _PROFILING
		ZoneScopedN("FlushPendingReslots");
#endif
		mRendererFrontend->FlushPendingReslots();
	}

	// Dispatch compile jobs for any queued/deferred reload requests.
	// Returns immediately — jobs run on worker threads.
	{
#ifdef _PROFILING
		ZoneScopedN("FlushPendingReloads");
#endif
		mRendererFrontend->FlushPendingReloads();
	}

	// Poll for shader file changes — throttled to every 30 frames (~2x/sec at 60fps)
	// to avoid 543μs of filesystem stat overhead per frame.
	if (m_renderMode == RenderMode::EDITOR && (++m_shaderWatchFrameCounter % 30 == 0))
	{
#ifdef _PROFILING
		ZoneScopedN("ShaderWatcher::Poll");
#endif
		m_shaderWatcher.Poll();
	}

	// Release GPU handles for retired resources, then hand back for CPU eviction.
	{
#ifdef _PROFILING
		ZoneScopedN("GPU Resource Release");
#endif
		for (auto& [type, resource] : mResourceGpuSync->TakePendingReleases())
		{
			if (resource->GetReferenceCount() > 0) continue; // re-acquired since queuing; skip
			importer->Release(type, resource, mRendererFrontend);
			resource->SetState(ResourceState::CPU_READY);
			mResourceGpuSync->EvictResource(type, resource);
		}
	}

	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleRenderer3D::Update(float dt)
{
	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleRenderer3D::PostUpdate(float dt)
{
#ifdef _PROFILING
	ZoneScoped;
#endif

	const SceneRenderData& sceneData = mModuleScene->GetRenderData();

	// While LoadSceneAsync is in flight, the worker thread is mutating the registry and
	// creating CMaterial/ResourceMaterial entries (assigning instance-pool slots). Any
	// view<>() iteration here races those mutations (EnTT is not thread-safe), and any
	// material whose internalID is read mid-construction can index out-of-bounds into
	// VulkanShader::instanceStates in DrawGeometryBatched. Drop all scene render state
	// for this frame; only the UI pass will draw.
	const bool isLoadingScene = mModuleScene->IsLoadingScene();
	if (isLoadingScene)
	{
		mMeshAABBCache.clear();
		mRendererFrontend->SetOutlinedGeometries({});
		mRendererFrontend->SetWireframeInstances(WireframeMesh::Cube, {});
		mRendererFrontend->SetWireframeInstances(WireframeMesh::Sphere, {});
		mRendererFrontend->SetWireframeInstances(WireframeMesh::Pyramid, {});
		mRendererFrontend->SetWireframeInstances(WireframeMesh::Cone, {});
		mRendererFrontend->SetWireframeInstances(WireframeMesh::Bone, {});
		mRendererFrontend->SetWireframeInstances(WireframeMesh::Joint, {});
		mRendererFrontend->SetCameraFrustums({});
		mRendererFrontend->SetDebugLines({});
	}

	// Video surfaces: hand each playing CVideoPlayer's latest frame to the renderer, which owns
	// the dynamic texture and binds it into the sibling material's targetSlot. This loop is the
	// one place renderer↔video coupling lives — everything below SubmitDynamicSurface is ECS-free.
	// ReconcileDynamicSurfaces drops surfaces whose player stopped / was removed. Skipped while a
	// scene loads (registry race).
	if (!isLoadingScene && sceneData.registry)
	{
		ResourceMaterial* defaultMat = mResourceProvider->GetDefaultMaterial();
		auto videoView = sceneData.registry->view<CVideoPlayer>();
		for (auto [entity, player] : videoView.each())
		{
			const auto* info   = sceneData.registry->try_get<CEntityInfo>(entity);
			const uint32_t goUID = info ? info->id : 0u;
			const auto* matC   = sceneData.registry->try_get<CMaterial>(entity);
			ResourceMaterial* material = matC ? matC->material : nullptr;

			// Only hand over pixels when a fresh frame is waiting; otherwise pass null to keep the
			// surface alive without re-uploading. We own frameDirty, so we clear it on consumption.
			const uint8_t* pixels = (player.frameDirty && player.latestFrame.pixels)
			                      ? player.latestFrame.pixels : nullptr;
			const bool consumed = mRendererFrontend->SubmitDynamicSurface(
				goUID, pixels, player.latestFrame.width, player.latestFrame.height,
				player.targetSlot, material, defaultMat);
			if (consumed)
				player.frameDirty = false;
		}
		mRendererFrontend->ReconcileDynamicSurfaces();
	}

	m_totalTime += dt;

	RenderPacket packet{};
	packet.deltaTime    = dt;
	packet.totalTime    = m_totalTime;
	packet.editorCamera = (m_renderMode == RenderMode::EDITOR) ? mModuleCamera3D->GetCamera() : nullptr;
	packet.gameCamera   = sceneData.gameCamera;

	// EDITOR mode: apply the game viewport's panel aspect ratio (written by GameViewport::Draw()
	// at end of the previous frame's UI pass) before frustum build and DrawFrame().
	// This overrides the authored value set by CCamera::OnUpdate() earlier this frame.
	// In GAME mode gameViewportAspect stays 0, so the authored value is used as-is.
	if (m_renderMode == RenderMode::EDITOR && packet.gameCamera && mModuleScene->gameViewportAspect > 0.0f)
		packet.gameCamera->SetAspectRatio(mModuleScene->gameViewportAspect);

	// Editor-only: selection outline.
	if (m_renderMode == RenderMode::EDITOR && !isLoadingScene)
	{
		std::vector<GeometryRenderData> outlinedGeometries;
		for (auto go : sceneData.selectedObjects)
		{
			auto* m = go.TryGetComponent<CMesh>();
			if (!m || !m->mesh) continue;

			GeometryRenderData data{};
			if (auto* t = go.TryGetComponent<CTransform>()) data.model = t->worldMatrix;
			data.geometry = m->mesh;

			// Without this the outline traces the BIND pose while the mesh deforms --
			// a halo standing beside the character rather than around it.
			ApplySkinningToGeometry(*sceneData.registry, go.GetEntity(), *m->mesh, data);

			outlinedGeometries.push_back(data);
		}
		mRendererFrontend->SetOutlinedGeometries(outlinedGeometries);
	}

	// Build world-space AABB cache for frustum culling.
	// Runs in EDITOR mode (always needed for bounding box overlays) and in any
	// mode when frustum culling is enabled. Without this, BuildRenderPacket would
	// find an empty cache and silently skip all culling in GAME mode.
	//
	// While LoadSceneAsync is in flight the cache was already cleared at the top of
	// PostUpdate and iteration is skipped here — see the comment near isLoadingScene.
	if (!isLoadingScene && (m_renderMode == RenderMode::EDITOR || frustumCullingEnabled))
	{
#ifdef _PROFILING
		ZoneScopedN("AABB Cache");
#endif
		if (sceneData.registry)
		{
			std::vector<WireframeInstance> boundingBoxes;
			auto view = sceneData.registry->view<CMesh, CTransform>();

			for (auto [entity, meshComp, transform] : view.each())
			{
				if (!meshComp.mesh) continue;
				if (meshComp.mesh->internalID == INVALID_ID) continue;

				const auto* info = sceneData.registry->try_get<CEntityInfo>(entity);
				const uint32_t id = info ? info->id : 0u;

				const glm::vec3 localMin     = meshComp.mesh->localAABBMin;
				const glm::vec3 localMax     = meshComp.mesh->localAABBMax;
				const glm::mat4& worldMatrix = transform.worldMatrix;

				glm::vec3 worldMin, worldMax;

				// A skinned mesh's TRANSFORM never changes while it animates -- the pose
				// moves, not the object -- so the m_worldDirty cache would freeze it on
				// frame one's box and the character would pop out of view mid-animation.
				// Skinned meshes recompute every frame instead.
				const CAnimator* animator = nullptr;
				if (meshComp.mesh->hasSkinning)
				{
					const auto* hierarchy = sceneData.registry->try_get<CHierarchy>(entity);
					if (hierarchy && hierarchy->parent != entt::null)
						animator = sceneData.registry->try_get<CAnimator>(hierarchy->parent);
				}
				const bool isSkinned = animator && !animator->GetPalette().empty();

				// The box the corners are built from. For a skinned mesh both the extents
				// and the placing matrix come from the pose, not the bind data.
				glm::vec3 srcMin    = localMin;
				glm::vec3 srcMax    = localMax;
				glm::mat4 srcMatrix = worldMatrix;

				if (isSkinned)
				{
					glm::vec3 skinnedMin, skinnedMax;
					if (nous::engine::animation_system::ComputeSkinnedBounds(
							meshComp.mesh->boneAABBMin, meshComp.mesh->boneAABBMax,
							animator->GetPalette(), skinnedMin, skinnedMax))
					{
						srcMin = skinnedMin;
						srcMax = skinnedMax;

						// The palette outputs MODEL space, so the box is placed by the
						// animator root's world matrix -- the same matrix the skinned
						// draw uses, not this child's.
						if (const auto* rootTransform = sceneData.registry->try_get<CTransform>(
								sceneData.registry->get<CHierarchy>(entity).parent))
						{
							srcMatrix = rootTransform->worldMatrix;
						}
					}
				}

				// Only recompute the 8-corner AABB transform when the world matrix changed.
				// Static objects reuse the cached result from the previous frame.
				if (isSkinned || transform.m_worldDirty || mMeshAABBCache.find(id) == mMeshAABBCache.end())
				{
					const glm::vec3 corners[8] = {
						glm::vec3(srcMatrix * glm::vec4(srcMin.x, srcMin.y, srcMin.z, 1.0f)),
						glm::vec3(srcMatrix * glm::vec4(srcMax.x, srcMin.y, srcMin.z, 1.0f)),
						glm::vec3(srcMatrix * glm::vec4(srcMin.x, srcMax.y, srcMin.z, 1.0f)),
						glm::vec3(srcMatrix * glm::vec4(srcMax.x, srcMax.y, srcMin.z, 1.0f)),
						glm::vec3(srcMatrix * glm::vec4(srcMin.x, srcMin.y, srcMax.z, 1.0f)),
						glm::vec3(srcMatrix * glm::vec4(srcMax.x, srcMin.y, srcMax.z, 1.0f)),
						glm::vec3(srcMatrix * glm::vec4(srcMin.x, srcMax.y, srcMax.z, 1.0f)),
						glm::vec3(srcMatrix * glm::vec4(srcMax.x, srcMax.y, srcMax.z, 1.0f)),
					};

					worldMin = corners[0];
					worldMax = corners[0];
					for (const auto& c : corners)
					{
						worldMin = glm::min(worldMin, c);
						worldMax = glm::max(worldMax, c);
					}

					mMeshAABBCache[id] = { worldMin, worldMax };
					transform.m_worldDirty = false;
				}
				else
				{
					const auto& cached = mMeshAABBCache[id];
					worldMin = cached.first;
					worldMax = cached.second;
				}

				// Editor-only: generate OBB and AABB overlay geometry.
				if (m_renderMode == RenderMode::EDITOR && mRendererFrontend->showBoundingBoxes)
				{
					// srcMin/srcMax/srcMatrix, not the bind-pose local box: for a skinned
					// mesh these are the posed extents and the animator root's matrix, so
					// the OBB follows the animation like the AABB does.
					const glm::vec3 localCenter  = (srcMin + srcMax) * 0.5f;
					const glm::vec3 localExtents = srcMax - srcMin;
					glm::mat4 obbTransform = srcMatrix
						* glm::translate(glm::mat4(1.0f), localCenter)
						* glm::scale(glm::mat4(1.0f), localExtents);

					const glm::vec3 worldCenter  = (worldMin + worldMax) * 0.5f;
					const glm::vec3 worldExtents = worldMax - worldMin;

					boundingBoxes.emplace_back(obbTransform, glm::vec4(0.3f, 0.6f, 1.0f, 1.0f)); // blue
					glm::mat4 aabbTransform = glm::translate(glm::mat4(1.0f), worldCenter)
						* glm::scale(glm::mat4(1.0f), worldExtents);
					boundingBoxes.emplace_back(aabbTransform, glm::vec4(1.0f, 0.4f, 0.1f, 1.0f)); // orange-red
				}
			}

			if (m_renderMode == RenderMode::EDITOR)
				mRendererFrontend->SetWireframeInstances(WireframeMesh::Cube, boundingBoxes);
		}
	}
	else
	{
		mMeshAABBCache.clear();
	}

	// Editor-only: camera frustum overlays.
	if (m_renderMode == RenderMode::EDITOR && !isLoadingScene)
	{
#ifdef _PROFILING
		ZoneScopedN("Frustum Build");
#endif
		std::vector<CameraFrustumData> frustums;

		if (sceneData.registry)
		{
			auto view = sceneData.registry->view<CCamera, CTransform>();
			for (auto [entity, cam, transform] : view.each())
			{
				const float vfovRad     = glm::radians(cam.fov);
				const float halfTan     = std::tan(vfovRad * 0.5f);
				const float halfH_near  = cam.nearPlane * halfTan;
				const float halfH_far   = cam.farPlane  * halfTan;
				// Use viewport-driven aspect ratio when available (EDITOR mode with game panel open);
				// fall back to the authored field for GAME mode or before the first panel draw.
				const float effectiveAspect = (mModuleScene->gameViewportAspect > 0.0f)
				    ? mModuleScene->gameViewportAspect : cam.aspectRatio;
				const float halfW_near  = halfH_near * effectiveAspect;
				const float halfW_far   = halfH_far  * effectiveAspect;

				const glm::vec3 pos   = transform.position;
				const glm::vec3 fwd   = transform.GetForward();
				const glm::vec3 up    = transform.GetUp();
				const glm::vec3 right = transform.GetRight();

				const glm::vec3 nearCenter = pos + fwd * cam.nearPlane;
				const glm::vec3 farCenter  = pos + fwd * cam.farPlane;

				CameraFrustumData fdata{};
				// Near quad: TL, TR, BR, BL
				fdata.corners[0] = nearCenter + up * halfH_near - right * halfW_near;
				fdata.corners[1] = nearCenter + up * halfH_near + right * halfW_near;
				fdata.corners[2] = nearCenter - up * halfH_near + right * halfW_near;
				fdata.corners[3] = nearCenter - up * halfH_near - right * halfW_near;
				// Far quad: TL, TR, BR, BL
				fdata.corners[4] = farCenter  + up * halfH_far  - right * halfW_far;
				fdata.corners[5] = farCenter  + up * halfH_far  + right * halfW_far;
				fdata.corners[6] = farCenter  - up * halfH_far  + right * halfW_far;
				fdata.corners[7] = farCenter  - up * halfH_far  - right * halfW_far;

				// Main camera: yellow; secondary cameras: green.
				fdata.color = cam.isMainCamera
					? glm::vec4(1.0f, 0.85f, 0.0f, 1.0f)
					: glm::vec4(0.2f, 0.9f,  0.2f, 1.0f);

				frustums.push_back(fdata);
			}
		}

		mRendererFrontend->SetCameraFrustums(frustums);
	}

	// Editor-only: point light debug spheres.
	//   Small marker sphere at every point light's position (always visible).
	//   Larger range sphere shown only when the light's GameObject is selected.
	if (m_renderMode == RenderMode::EDITOR && !isLoadingScene)
	{
#ifdef _PROFILING
		ZoneScopedN("Light Debugs");
#endif
		std::vector<WireframeInstance> pointLightDebugs;

		if (sceneData.registry)
		{
			constexpr float c_markerRadius = 0.25f;

			auto view = sceneData.registry->view<CLight, CTransform>();
			for (auto [entity, light, transform] : view.each())
			{
				if (light.type != LightType::Point) continue;

				const glm::vec4 color = glm::vec4(light.color, 1.0f);

				// Always: small fixed-size marker sphere.
				pointLightDebugs.emplace_back(
					glm::translate(glm::mat4(1.0f), transform.position) *
					glm::scale(glm::mat4(1.0f), glm::vec3(c_markerRadius)),
					color);

				// Only for the selected light: range sphere.
				if (sceneData.primaryObject.IsValid() &&
				    sceneData.primaryObject.GetEntity() == entity)
				{
					pointLightDebugs.emplace_back(
						glm::translate(glm::mat4(1.0f), transform.position) *
						glm::scale(glm::mat4(1.0f), glm::vec3(light.range)),
						color);
				}
			}
		}

		mRendererFrontend->SetWireframeInstances(WireframeMesh::Sphere, pointLightDebugs);

		// Directional light debug pyramids.
		std::vector<WireframeInstance> dirLightDebugs;
		// Spot light debug cones.
		std::vector<WireframeInstance> spotLightDebugs;

		if (sceneData.registry)
		{
			constexpr float c_markerRadius = 0.25f;

			auto lightView2 = sceneData.registry->view<CLight, CTransform>();
			for (auto [entity, light, transform] : lightView2.each())
			{
				const bool isSelected = sceneData.primaryObject.IsValid() &&
				                        sceneData.primaryObject.GetEntity() == entity;
				const glm::vec4 color = glm::vec4(light.color, 1.0f);

				if (light.type == LightType::Directional)
				{
					dirLightDebugs.emplace_back(
						glm::translate(glm::mat4(1.0f), transform.position) *
						glm::mat4_cast(transform.orientation),
						color);
				}
				else if (light.type == LightType::Spot)
				{
					const float outerRad  = glm::radians(light.outerAngle);
					const float coneScale = std::tan(outerRad) * light.range;

					// Marker: small fixed cone at the spot position — always drawn.
					spotLightDebugs.emplace_back(
						glm::translate(glm::mat4(1.0f), transform.position) *
						glm::mat4_cast(transform.orientation) *
						glm::scale(glm::mat4(1.0f), glm::vec3(c_markerRadius)),
						color);

					// Full cone: scaled to outerAngle + range — only when selected.
					if (isSelected)
					{
						spotLightDebugs.emplace_back(
							glm::translate(glm::mat4(1.0f), transform.position) *
							glm::mat4_cast(transform.orientation) *
							glm::scale(glm::mat4(1.0f), glm::vec3(coneScale, light.range, coneScale)),
							color);
					}
				}
			}
		}

		mRendererFrontend->SetWireframeInstances(WireframeMesh::Pyramid, dirLightDebugs);
		mRendererFrontend->SetWireframeInstances(WireframeMesh::Cone, spotLightDebugs);
	}

	// ── Skeleton debug lines (one instanced segment per bone with a parent) ──────
	//
	// Built HERE rather than in an animation module on purpose: ModuleRenderer3D is
	// allowed to know ECS components, so the lines never cross a Systems/ ->
	// Modules/ edge. CAnimator keeps its pose internal and this is the only reader.
	//
	// CAnimator's globals are MODEL space, so each joint is composed with the
	// owning object's world matrix before the segment is built.
	if (m_renderMode == RenderMode::EDITOR && mRendererFrontend->showSkeletons && sceneData.registry)
	{
#ifdef _PROFILING
		ZoneScopedN("BuildSkeletonDebugShards");
#endif
		std::vector<WireframeInstance> boneShards;
		std::vector<WireframeInstance> jointMarkers;
		std::vector<glm::vec3>         jointWorld;   // reused per animator
		std::vector<float>             boneLength;  // distance to parent, 0 for roots
		std::vector<float>             jointScale;  // shortest bone touching each joint
		std::vector<float>             jointRadius; // per-joint marker radius

		auto animView = sceneData.registry->view<CAnimator, CTransform>();
		for (auto entity : animView)
		{
			const CAnimator&  animator = animView.get<CAnimator>(entity);
			const auto&       globals  = animator.GetBoneGlobals();

			if (globals.empty() || !animator.skeleton)
				continue;

			const auto& parents = animator.skeleton->skeleton.parents;
			if (parents.size() != globals.size())
				continue;   // slot swapped mid-frame; skip rather than index past the end

			const glm::mat4& world = animView.get<CTransform>(entity).worldMatrix;

			// Column 3 of a bone global IS its translation (w == 1), so this is the
			// world-space joint position without building a vec4 or multiplying the
			// whole matrix. Computed once here and read by both the segments and the
			// joint markers.
			jointWorld.clear();
			jointWorld.reserve(globals.size());
			for (const glm::mat4& g : globals)
				jointWorld.emplace_back(glm::vec3(world * g[3]));

			// PASS 1 — measure the rig before drawing any of it.
			//
			// The shard's thickness is the rig's marker radius, which needs the MEAN
			// bone length, which is only known once every bone has been visited. So
			// measuring has to finish before emitting starts; a single fused loop
			// would have to guess the radius for the bones it reached first.
			// Each bone's length is kept so pass 2 does not recompute it.
			float    boneLengthTotal = 0.0f;
			uint32_t boneLengthCount = 0;

			boneLength.assign(globals.size(), 0.0f);

			for (size_t i = 0; i < globals.size(); ++i)
			{
				if (parents[i] < 0)
					continue;   // root bone has no segment to draw

				const float length = glm::length(
					jointWorld[i] - jointWorld[static_cast<size_t>(parents[i])]);
				if (length < 1e-6f)
					continue;   // coincident joints — a _End terminator, or a bad bind

				boneLength[i]    = length;
				boneLengthTotal += length;
				++boneLengthCount;
			}

			// Size is DERIVED from the rig's own mean bone length, never a constant:
			// Mixamo exports are in centimetres (hips at y ~= 104) while a
			// metres-authored model puts them at y ~= 1.04, so any fixed radius is
			// invisible on one and swallows the rig on the other.
			if (boneLengthCount == 0)
				continue;

			const float markerRadius = (boneLengthTotal / static_cast<float>(boneLengthCount)) * 0.12f;

			// PASS 2 — per-joint radii, and the markers that use them directly.
			//
			// Size is PER JOINT, scaled by the SHORTEST bone touching that joint. A
			// Mixamo hand packs ~20 finger joints into the space of one forearm, so
			// rig-mean markers there overlap into a solid blob that hides the bones
			// underneath — the densest part of the rig became the least readable,
			// which is exactly backwards.
			//
			// "Shortest touching", not "the bone that arrives": a knuckle is the far
			// end of the metacarpal spanning the whole palm AND the near end of a tiny
			// finger bone. Sized by the arriving bone alone it swells to palm scale and
			// swallows the fingers hanging off it. Every joint that reads as too big is
			// this shape — wrist, elbow, knuckle — so each bone pulls its PARENT down
			// as well as sizing its own joint. In uniform regions like the spine the
			// arriving and leaving bones already match, so nothing moves.
			jointScale.assign(globals.size(), 0.0f);

			for (size_t i = 0; i < globals.size(); ++i)
			{
				const float length = boneLength[i];
				if (length < 1e-6f)
					continue;   // root, or a coincident-joint terminator

				const size_t parent = static_cast<size_t>(parents[i]);

				// 0 is the "unset" marker, so the first bone to reach a joint claims it
				// outright and later ones can only shrink it.
				jointScale[i]      = (jointScale[i]      < 1e-6f) ? length : glm::min(jointScale[i],      length);
				jointScale[parent] = (jointScale[parent] < 1e-6f) ? length : glm::min(jointScale[parent], length);
			}

			// Clamped at both ends: the cap keeps a long spine bone from ballooning
			// past the rig-wide look, and the floor keeps zero-length _End terminators
			// visible instead of collapsing them to nothing.
			jointRadius.assign(globals.size(), markerRadius);

			for (size_t i = 0; i < jointWorld.size(); ++i)
			{
				const float ownRadius = (jointScale[i] > 1e-6f)
					? jointScale[i] * 0.12f
					: markerRadius;   // isolated joint — no bone at all to measure

				jointRadius[i] = glm::clamp(ownRadius, markerRadius * 0.25f, markerRadius);

				jointMarkers.emplace_back(
					glm::translate(glm::mat4(1.0f), jointWorld[i]) *
					glm::scale(glm::mat4(1.0f), glm::vec3(jointRadius[i])),
					glm::vec4(1.0f, 0.8f, 0.2f, 1.0f));   // amber
			}

			// PASS 3 — emit the shards, sized to the joints they connect.
			for (size_t i = 0; i < globals.size(); ++i)
			{
				const float length = boneLength[i];
				if (length < 1e-6f)
					continue;   // root, or a coincident-joint terminator

				const size_t     parent = static_cast<size_t>(parents[i]);
				const glm::vec3& head   = jointWorld[parent];
				const glm::vec3& tail   = jointWorld[i];

				// The collar takes the SMALLER of the two joints it spans, so a shard
				// can never be wider than either sphere it connects. Taking only its
				// own end lets a long bone hanging off a small joint flare out past it
				// — which is the same "fat bones, tiny spheres" mismatch one level down.
				const float collar = glm::min(jointRadius[i], jointRadius[parent]);

				// NON-UNIFORM on purpose: Y spans the bone, X/Z carry the collar.
				// Scaling uniformly by length would make a long thigh bone as fat as it
				// is long while finger bones vanish.
				boneShards.emplace_back(
					glm::translate(glm::mat4(1.0f), head) *
					glm::mat4_cast(glm::rotation(glm::vec3(0.0f, 1.0f, 0.0f), (tail - head) / length)) *
					glm::scale(glm::mat4(1.0f), glm::vec3(collar, length, collar)),
					glm::vec4(0.2f, 1.0f, 0.4f, 1.0f));   // green
			}
		}

		mRendererFrontend->SetWireframeInstances(WireframeMesh::Bone,  boneShards);
		mRendererFrontend->SetWireframeInstances(WireframeMesh::Joint, jointMarkers);
	}

	// Normals visualization — SELECTED OBJECT ONLY. Per-vertex normals for every
	// visible mesh would be hundreds of thousands of segments rebuilt per frame; one
	// selected mesh is a fixed budget and is what you actually want while inspecting.
	if (m_renderMode == RenderMode::EDITOR && !isLoadingScene &&
	    mRendererFrontend->showNormals && sceneData.registry)
	{
#ifdef _PROFILING
		ZoneScopedN("BuildNormalDebugLines");
#endif
		// Reused across frames: the overlay rebuilds every frame while enabled, and a
		// fresh vector per frame is a multi-megabyte allocation each time.
		m_normalLines.clear();

		for (auto go : sceneData.selectedObjects)
		{
			auto* meshComp = go.TryGetComponent<CMesh>();
			if (!meshComp || !meshComp->mesh || meshComp->mesh->vertices.empty())
				continue;

			const ResourceMesh& mesh = *meshComp->mesh;

			GeometryRenderData data{};
			if (auto* t = go.TryGetComponent<CTransform>()) data.model = t->worldMatrix;
			ApplySkinningToGeometry(*sceneData.registry, go.GetEntity(), mesh, data);

			// Length is DERIVED from the mesh's own bind extent, never a constant --
			// the same centimetres-versus-metres problem that forced the joint-marker
			// radius to be derived. A fixed length is invisible on a Mixamo character
			// and swallows a metre-scale prop.
			const glm::vec3 extent = mesh.localAABBMax - mesh.localAABBMin;
			const float     length = glm::max(glm::max(extent.x, extent.y), extent.z) * 0.02f;

			// Stride, not truncation. A truncated field shows normals on part of the
			// mesh and none on the rest, which reads as "skinning failed over there".
			//
			// The budget is a DISPLAY budget, deliberately far below the vertex buffer's
			// capacity. Striding at the buffer limit meant ~20k segments rebuilt, skinned
			// and uploaded every frame -- several MB per frame, which costs more than it
			// shows: at that density the overlay is a solid mass of lines anyway.
			constexpr size_t k_MaxNormalSegments = 4096;
			static_assert(k_MaxNormalSegments * 2 <= c_maxDebugLineVertices,
				"Normal segments must fit the debug line buffer.");

			const size_t capacity = k_MaxNormalSegments;
			const size_t stride   = (mesh.vertices.size() + capacity - 1) / capacity;

			const bool skinned = data.palette != nullptr;

			const auto emit = [&](const glm::vec3& localPos, const glm::vec3& localNrm)
			{
				const glm::vec3 p = glm::vec3(data.model * glm::vec4(localPos, 1.0f));
				const glm::vec3 n = glm::normalize(glm::mat3(data.model) * localNrm);

				Vertex3D a{}, b{};
				a.position = p;
				b.position = p + n * length;
				m_normalLines.push_back(a);
				m_normalLines.push_back(b);
			};

			if (skinned)
			{
				// SkinVertices takes de-interleaved spans and deliberately does not
				// name Vertex3D, to keep AnimationSystem dependency-free -- so scatter
				// into reusable members rather than forking the tested maths here.
				m_normalScratchPos.clear();
				m_normalScratchNrm.clear();
				m_normalScratchIDs.clear();
				m_normalScratchWts.clear();

				for (size_t i = 0; i < mesh.vertices.size(); i += stride)
				{
					const Vertex3D& v = mesh.vertices[i];
					m_normalScratchPos.push_back(v.position);
					m_normalScratchNrm.push_back(v.normal);
					m_normalScratchIDs.push_back(v.boneIDs);
					m_normalScratchWts.push_back(v.boneWeights);
				}

				m_normalScratchOutPos.assign(m_normalScratchPos.size(), glm::vec3(0.0f));
				m_normalScratchOutNrm.assign(m_normalScratchPos.size(), glm::vec3(0.0f));

				if (!nous::engine::animation_system::SkinVertices(
						*data.palette, m_normalScratchPos, m_normalScratchNrm,
						m_normalScratchIDs, m_normalScratchWts,
						m_normalScratchOutPos, m_normalScratchOutNrm))
					continue;

				for (size_t i = 0; i < m_normalScratchOutPos.size(); ++i)
					emit(m_normalScratchOutPos[i], m_normalScratchOutNrm[i]);
			}
			else
			{
				for (size_t i = 0; i < mesh.vertices.size(); i += stride)
					emit(mesh.vertices[i].position, mesh.vertices[i].normal);
			}
		}

		// By const ref, not moved: moving would hand away this buffer's capacity and
		// leave the member empty, so the allocation would come back every frame.
		mRendererFrontend->SetDebugLines(m_normalLines);
	}
	else if (m_renderMode == RenderMode::EDITOR && !isLoadingScene)
	{
		mRendererFrontend->SetDebugLines({});
	}

	{
#ifdef _PROFILING
		ZoneScopedN("BuildRenderPacket");
#endif
		if (!BuildRenderPacket(&packet, sceneData) || mIsMinimized)
			return UpdateStatus::CONTINUE;
	}

	{
#ifdef _PROFILING
		ZoneScopedN("DrawFrame");
#endif
		FrameResult result = mRendererFrontend->DrawFrame(&packet);

		switch (result)
		{
			case FrameResult::SUCCESS:
				break;

			case FrameResult::SKIPPED:
				NOUS_INFO_C(CURRENT_CHANNEL, "Frame skipped (window resize or swapchain recreation).");
				break;

			case FrameResult::ERROR:
				NOUS_FATAL_C(CURRENT_CHANNEL, "Fatal rendering error. Aborting application.");
				return UpdateStatus::ERROR;
		}
	}

	return UpdateStatus::CONTINUE;
}

bool ModuleRenderer3D::CleanUp()
{
    // Release frame-scoped Vulkan objects (waits for GPU, frees command buffers
    // and framebuffers).  If the Editor already called this, it is a no-op.
    mRendererFrontend->ReleaseFrameResources();

    // Destroy renderer-owned dynamic surfaces (GPU is idle after ReleaseFrameResources, and the
    // owning materials are still alive — DestroyDynamicSurfaces restores their original slot tex).
    mRendererFrontend->DestroyDynamicSurfaces();

    // Destroy all GameObjects BEFORE freeing GPU resources.  Component
    // OnDestroy callbacks (CMesh, CMaterial) need the ResourceManager and
    // its Resource objects to still be alive so they can safely decrement
    // reference counts via UnloadResource().
    mModuleScene->ClearScene();

    // Destroy all GPU resources (textures, shaders, meshes, materials).
    // Safe because ReleaseFrameResources() already freed the CBs/FBs that
    // referenced these objects, and the scene has been cleared above so no
    // component still holds a reference to any Resource.
    mResourceGpuSync->ClearResources(mRendererFrontend);

    // Tear down the remaining Vulkan infrastructure (buffers, sync objects,
    // renderpasses, swapchain, device).
	mRendererFrontend->Shutdown();

	NOUS_INFO_C(CURRENT_CHANNEL, "Renderer Frontend shutdown was successful.");

	return true;
}

void ModuleRenderer3D::OnEvent(const Event& event)
{
	switch (event.type)
	{
		case EventType::WINDOW_RESIZED:
		{
			NOUS_INFO_C(CURRENT_CHANNEL, "Event Received: WINDOW_RESIZED (%d) - Context: %d, %d",
					  static_cast<int>(event.type),
					  event.ctx.i32[0],
					  event.ctx.i32[1]);

			mRendererFrontend->OnResized(event.ctx.i32[0], event.ctx.i32[1]);

			break;
		}
		case EventType::WINDOW_MINIMIZED:
		{
			mIsMinimized = event.ctx.u8[0] != 0;
			break;
		}
        case EventType::NONE:
        case EventType::TEST:
        case EventType::KEY_PRESSED:
        case EventType::SWAP_TEXTURE:
        case EventType::DROP_FILE:
        case EventType::INPUT_EVENT:
        case EventType::IMGUI_RECREATION:
        case EventType::KEY_RELEASED:
        case EventType::MOUSE_BUTTON:
        case EventType::MOUSE_MOVED:
        case EventType::FILE_DROPPED:
        case EventType::ENGINE_SHUTDOWN:
        case EventType::IMGUI_RECREATE:
        case EventType::CUSTOM:
            break;
    }
}

RendererFrontend* ModuleRenderer3D::GetRendererFrontend() const
{
	return mRendererFrontend;
}

IGPUResourceFactory* ModuleRenderer3D::GetGPUFactory() const
{
	return mRendererFrontend;
}

void ModuleRenderer3D::LoadShadersFromManifest()
{
	JsonObject root = JsonFile::LoadFromFile("Library/Shaders/shader_manifest.json");
	if (root.IsEmpty())
	{
		NOUS_FATAL_C(CURRENT_CHANNEL,
			"Library/Shaders/shader_manifest.json not found. "
			"Run the editor once to generate it before packaging the game.");
		return;
	}

	auto loadShader = [&](const char* key, const char* assetPath)
	{
		JsonObject entry = root.GetObject(key);
		if (entry.IsEmpty()) { NOUS_ERROR_C(CURRENT_CHANNEL, "shader_manifest.json: missing entry '%s'.", key); return; }

		const uint32_t      uid     = static_cast<uint32_t>(entry.GetDouble("uid", 0.0));
		const std::string libPath = entry.GetString("libraryPath");

		if (uid == 0 || libPath.empty())
		{ NOUS_ERROR_C(CURRENT_CHANNEL, "shader_manifest.json: invalid data for '%s'.", key); return; }

		mResourceLoader->CreateResourceFromLibrary(
			uid, ResourceType::SHADER, nous::engine::filesystem::GetFilename(assetPath), assetPath, libPath);
	};

	loadShader("MaterialShader",   "Assets/Shaders/BuiltIn.MaterialShader.glsl");
	loadShader("BackgroundShader", "Assets/Shaders/BuiltIn.BackgroundShader.glsl");

	NOUS_INFO_C(CURRENT_CHANNEL, "Built-in shaders loaded from shader_manifest.json.");
}

bool ModuleRenderer3D::BuildRenderPacket(RenderPacket* packet, const SceneRenderData& sceneData)
{
#ifdef _PROFILING
	ZoneScopedN("BuildRenderPacket");
#endif
	if (!sceneData.hasActiveScene)
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "Active scene is not defined. Render packet will not be built.");
		return false;
	}

	packet->geometries.clear();
	packet->gameGeometries.clear();
	packet->hasDirectionalLight   = false;
	packet->activePointLightCount = 0;
	packet->activeSpotLightCount  = 0;

	// Don't iterate the registry while LoadSceneAsync is mutating it on a worker thread.
	// Returning here leaves the packet empty — UI pass still draws, but no scene geometry
	// is fed to DrawGeometryBatched (which would otherwise crash on a torn material->internalID).
	if (mModuleScene->IsLoadingScene())
		return true;

	// Per-pass frustum planes.
	// In EDITOR mode both frustums are ALWAYS applied (independent of frustumCullingEnabled).
	// In GAME mode the game frustum requires frustumCullingEnabled to be set explicitly.
	FrustumCulling::FrustumPlanes sceneFrustum{};
	FrustumCulling::FrustumPlanes gameFrustum{};
	bool hasSceneFrustum = false;
	bool hasGameFrustum  = false;

	// The frustum-selection, visibility and light-packing rules live in
	// RenderPacketPolicy.h so they can be unit-tested without a registry or a
	// camera (t_ModuleRenderer3D_RenderPacketPolicy).
	const bool isEditorMode = (m_renderMode == RenderMode::EDITOR);

	if (nous::engine::renderer::ShouldBuildSceneFrustum(isEditorMode, packet->editorCamera != nullptr))
	{
		const glm::mat4 vp = packet->editorCamera->GetProjectionMatrix()
		                   * packet->editorCamera->GetViewMatrix();
		sceneFrustum    = FrustumCulling::ExtractFrustumPlanes(vp);
		hasSceneFrustum = true;
	}
	// Game frustum: always active in EDITOR (per-frame preview culling);
	// opt-in via frustumCullingEnabled in GAME mode.
	if (nous::engine::renderer::ShouldBuildGameFrustum(isEditorMode, packet->gameCamera != nullptr,
	                                              frustumCullingEnabled))
	{
		const glm::mat4 vp = packet->gameCamera->GetProjectionMatrix()
		                   * packet->gameCamera->GetViewMatrix();
		gameFrustum    = FrustumCulling::ExtractFrustumPlanes(vp);
		hasGameFrustum = true;
	}

	if (!sceneData.registry) return false;

	auto meshView = sceneData.registry->view<CMesh, CTransform>();
	const auto totalMeshes = static_cast<size_t>(
		std::distance(meshView.begin(), meshView.end()));
	packet->geometries.reserve(totalMeshes);
	packet->gameGeometries.reserve(totalMeshes);

	for (auto [entity, mesh, transform] : meshView.each())
	{
		if (!mesh.mesh) continue;

		GeometryRenderData data{};

		const auto* info = sceneData.registry->try_get<CEntityInfo>(entity);
		data.objectUID   = info ? info->id : 0u;
		data.model       = transform.worldMatrix;
		data.geometry    = mesh.mesh;

		ApplySkinningToGeometry(*sceneData.registry, entity, *mesh.mesh, data);

		if (const auto* mat = sceneData.registry->try_get<CMaterial>(entity))
			data.material = mat->material;

		// Scene pass (editor camera frustum culling).
		if (m_renderMode == RenderMode::EDITOR)
		{
			// `hasSceneFrustum &&` keeps IsAABBVisible short-circuited: without it the
			// AABB test would run for every mesh every frame even with culling off.
			const auto sceneIt      = mMeshAABBCache.find(data.objectUID);
			const bool sceneMeasured = sceneIt != mMeshAABBCache.end();
			const bool passesScene   = nous::engine::renderer::IsGeometryVisible(
				hasSceneFrustum, sceneMeasured,
				hasSceneFrustum && sceneMeasured
					&& FrustumCulling::IsAABBVisible(sceneFrustum, sceneIt->second.first, sceneIt->second.second));
			if (passesScene)
				packet->geometries.emplace_back(data);

			// Game pass (game camera frustum culling).
			const auto gameIt       = mMeshAABBCache.find(data.objectUID);
			const bool gameMeasured = gameIt != mMeshAABBCache.end();
			const bool passesGame   = nous::engine::renderer::IsGeometryVisible(
				hasGameFrustum, gameMeasured,
				hasGameFrustum && gameMeasured
					&& FrustumCulling::IsAABBVisible(gameFrustum, gameIt->second.first, gameIt->second.second));
			if (passesGame)
				packet->gameGeometries.emplace_back(data);
		}
		else
		{
			// GAME mode: single pass — cull against game camera.
			const auto gameIt       = mMeshAABBCache.find(data.objectUID);
			const bool gameMeasured = gameIt != mMeshAABBCache.end();
			const bool passesGame   = nous::engine::renderer::IsGeometryVisible(
				hasGameFrustum, gameMeasured,
				hasGameFrustum && gameMeasured
					&& FrustumCulling::IsAABBVisible(gameFrustum, gameIt->second.first, gameIt->second.second));
			if (passesGame)
				packet->geometries.emplace_back(data);
		}
	}

	// ── Light gathering ───────────────────────────────────────────────────────────
	auto lightView = sceneData.registry->view<CLight, CTransform>();
	for (auto [entity, light, transform] : lightView.each())
	{
		if (light.type == LightType::Directional)
		{
			if (nous::engine::renderer::CanAcceptDirectionalLight(packet->hasDirectionalLight))
			{
				packet->directionalLight.direction =
					glm::vec4(nous::engine::renderer::LightForward(transform.orientation), 0.f);
				packet->directionalLight.color     =
					nous::engine::renderer::PackLightColor(light.color, light.intensity);
				packet->hasDirectionalLight        = true;
			}
			else
			{
				NOUS_WARN_C(CURRENT_CHANNEL,
					"Scene has more than one directional light; only the first is used.");
			}
		}
		else if (light.type == LightType::Point)
		{
			if (nous::engine::renderer::CanAcceptLight(packet->activePointLightCount, c_maxPointLights))
			{
				PointLight& pl = packet->pointLights[packet->activePointLightCount++];
				pl.position    = nous::engine::renderer::PackLightPosition(transform.position, light.range);
				pl.color       = nous::engine::renderer::PackLightColor(light.color, light.intensity);
			}
			else
			{
				const auto* info = sceneData.registry->try_get<CEntityInfo>(entity);
				NOUS_WARN_C(CURRENT_CHANNEL,
					"Point light limit (%u) reached; light on '%s' ignored.",
					c_maxPointLights, info ? info->name.c_str() : "<unknown>");
			}
		}
		else if (light.type == LightType::Spot)
		{
			if (nous::engine::renderer::CanAcceptLight(packet->activeSpotLightCount, c_maxSpotLights))
			{
				SpotLight& sl      = packet->spotLights[packet->activeSpotLightCount++];
				sl.position        = nous::engine::renderer::PackLightPosition(transform.position, light.range);
				sl.direction       = glm::vec4(nous::engine::renderer::LightForward(transform.orientation), 0.f);
				sl.color           = nous::engine::renderer::PackLightColor(light.color, light.intensity);
				sl.angles          = nous::engine::renderer::PackSpotAngles(light.innerAngle, light.outerAngle);
			}
			else
			{
				const auto* info = sceneData.registry->try_get<CEntityInfo>(entity);
				NOUS_WARN_C(CURRENT_CHANNEL,
					"Spot light limit (%u) reached; light on '%s' ignored.",
					c_maxSpotLights, info ? info->name.c_str() : "<unknown>");
			}
		}
	}

	return true;
}
