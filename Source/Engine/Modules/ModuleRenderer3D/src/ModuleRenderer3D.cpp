#include "Engine/Modules/ModuleRenderer3D/include/ModuleRenderer3D.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include "Engine/Modules/ModuleWindow/include/ModuleWindow.h"
#include "Engine/Modules/ModuleCamera3D/include/ModuleCamera3D.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"

#include "Engine/Renderer/Frontend/RendererFrontend.h"

#include "Engine/Modules/ModuleScene/include/SceneRenderData.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"
#include "Engine/Systems/ECS/Component/CLight/include/CLight.h"
#include "Engine/Systems/ECS/ECSInternalComponents.h"

#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/Core/Logger/LogChannel.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Systems/ResourceManager/Resource/Resource.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceTexture/include/ResourceTexture.h"
#include "Engine/Systems/ResourceManager/Importer/IImporterManager.h"
#include "Engine/Core/FileSystem/FileSystem.h"

#include <filesystem>
#include <parson.h>
#include "Engine/Systems/CameraSystem/Camera/include/Camera.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Utils/Math/FrustumCulling.h"


#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MODULE_RENDERER3D;

ModuleRenderer3D::ModuleRenderer3D(EventSystem* eventSystem, NOUS_Multithreading::NOUS_JobSystem* jobSystem,
	ModuleWindow* moduleWindow, ModuleCamera3D* moduleCamera,
	ModuleResourceManager* moduleResourceManager, ModuleScene* moduleScene) :
		Module(eventSystem, jobSystem), mModuleWindow(moduleWindow), mModuleCamera3D(moduleCamera),
		mModuleResourceManager(moduleResourceManager), mModuleScene(moduleScene)
{
	eventSystem->Subscribe(EventType::WINDOW_RESIZED, this);
	eventSystem->Subscribe(EventType::WINDOW_MINIMIZED, this);

	mRendererFrontend = NOUS_NEW<RendererFrontend>(MemoryTag::RENDERER);
}

ModuleRenderer3D::~ModuleRenderer3D()
{
	NOUS_DELETE(mRendererFrontend, MemoryTag::RENDERER);
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

	mRendererFrontend->InjectDependencies(mModuleWindow, eventSystem, JobSystem, mModuleResourceManager);

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
		// Capture return values so we can persist the manifest for GAME mode.
		Resource* matShader = mModuleResourceManager->CreateResource("Assets/Shaders/BuiltIn.MaterialShader.glsl");
		Resource* bgShader  = mModuleResourceManager->CreateResource("Assets/Shaders/BuiltIn.BackgroundShader.glsl");

		mModuleResourceManager->CreateResource("Assets/Shaders/BuiltIn.PickShader.glsl");
		mModuleResourceManager->CreateResource("Assets/Shaders/BuiltIn.OutlineShader.glsl");
		mModuleResourceManager->CreateResource("Assets/Shaders/BuiltIn.GridShader.glsl");
		mModuleResourceManager->CreateResource("Assets/Shaders/BuiltIn.BoundingBoxShader.glsl");

		if (matShader && bgShader)
			WriteShaderManifest(matShader, bgShader);
		else
			NOUS_WARN_C(CURRENT_CHANNEL, "Could not write shader_manifest.json — one or more built-in shaders failed to load.");
	}

	// Drain the initial upload queue — includes the default texture/material (queued
	// by ResourceManager::Start) and all shaders loaded above.  All must be
	// GPU_READY before the first frame renders.
	//
	// Materials depend on shader instance pools, but shaders are queued AFTER the
	// default material. Collect failed materials and retry after the full drain.
	IImporterManager* importer = mModuleResourceManager->GetImporterManager();
	std::vector<std::pair<ResourceType, Resource*>> deferredUploads;
	for (auto& [type, resource] : mModuleResourceManager->TakePendingUploads())
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
	ResourceTexture* defaultTex = mModuleResourceManager->GetDefaultTexture();
	if (defaultTex)
		defaultTex->generation = 0;
	ResourceTexture* whiteTex = mModuleResourceManager->GetWhiteTexture();
	if (whiteTex)
		whiteTex->generation = 0;
	ResourceTexture* blackTex = mModuleResourceManager->GetBlackTexture();
	if (blackTex)
		blackTex->generation = 0;
	ResourceTexture* flatNormalTex = mModuleResourceManager->GetFlatNormalTexture();
	if (flatNormalTex)
		flatNormalTex->generation = 0;

// Register all .glsl files in Assets/Shaders/ for hot reload (EDITOR only).
	// Changes are detected by Poll() in PreUpdate() and trigger a per-file reload.
	if (m_renderMode == RenderMode::EDITOR)
	{
		namespace fs = std::filesystem;
		std::error_code ec;
		int watchCount = 0;

		for (const auto& entry : fs::directory_iterator("Assets/Shaders", ec))
		{
			if (entry.path().extension() != ".glsl")
				continue;

			// Normalize to forward slashes — must match the paths in ResourceManager.
			const std::string normalizedPath = entry.path().generic_string();
			m_shaderWatcher.Watch(normalizedPath, [this, normalizedPath](const std::string&)
			{
				mRendererFrontend->ReloadShaderByPath(normalizedPath);
			});
			++watchCount;
		}

		if (!ec)
			NOUS_INFO_C(CURRENT_CHANNEL, "[ShaderHotReload] Watching %d shader file(s) in Assets/Shaders/.", watchCount);
		else
			NOUS_WARN_C(CURRENT_CHANNEL, "[ShaderHotReload] Could not open Assets/Shaders/ for watching: %s", ec.message().c_str());
	}

	return true;
}

UpdateStatus ModuleRenderer3D::PreUpdate(float dt)
{
	// Apply GPU swaps for compile jobs that completed since last frame (async path).
	// Must run before FlushPendingReloads and before DrawFrame — no renderpass open here.
	mRendererFrontend->FlushCompletedReloads();

	// Upload CPU_READY resources before processing reslots so that a newly-imported
	// custom shader is GPU_READY when FlushPendingReslots calls CreateMaterial.
	// Without this ordering, a reslot that fires in the same frame the target shader
	// is first uploaded would see the shader as not-GPU_READY and fall back to vsBase's
	// instance pool — causing a NULL descriptor-set error on the first draw call.
	IImporterManager* importer = mModuleResourceManager->GetImporterManager();
	for (auto& [type, resource] : mModuleResourceManager->TakePendingUploads())
	{
		if (!importer->Upload(type, resource, mRendererFrontend))
			NOUS_ERROR("ModuleRenderer3D::PreUpdate() — failed to upload resource '%s'.", resource->GetName().c_str());
		resource->SetState(ResourceState::GPU_READY);
	}

	// Process queued material shader changes (Inspector reslots). Must run after
	// FlushCompletedReloads (hot-reload GPU swaps) and TakePendingUploads (first-load
	// shader uploads) so the target shader is always GPU-ready when CreateMaterial runs.
	mRendererFrontend->FlushPendingReslots();

	// Dispatch compile jobs for any queued/deferred reload requests.
	// Returns immediately — jobs run on worker threads.
	mRendererFrontend->FlushPendingReloads();

	// Poll for shader file changes before any GPU work this frame (EDITOR only).
	// Safe here: previous frame's EndFrame has already been submitted; DrawFrame
	// is called later in PostUpdate, so no renderpass is open at this point.
	if (m_renderMode == RenderMode::EDITOR)
		m_shaderWatcher.Poll();

	// Release GPU handles for retired resources, then hand back for CPU eviction.
	for (auto& [type, resource] : mModuleResourceManager->TakePendingReleases())
	{
		if (resource->GetReferenceCount() > 0) continue; // re-acquired since queuing; skip
		importer->Release(type, resource, mRendererFrontend);
		resource->SetState(ResourceState::CPU_READY);
		mModuleResourceManager->EvictResource(type, resource);
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

	m_totalTime += dt;

	RenderPacket packet{};
	packet.deltaTime    = dt;
	packet.totalTime    = m_totalTime;
	packet.editorCamera = (m_renderMode == RenderMode::EDITOR) ? mModuleCamera3D->GetCamera() : nullptr;
	packet.gameCamera   = sceneData.gameCamera;

	// Editor-only: selection outline.
	if (m_renderMode == RenderMode::EDITOR)
	{
		std::vector<GeometryRenderData> outlinedGeometries;
		if (sceneData.selectedObject.IsValid() && sceneData.selectedObject.HasComponent<CMesh>())
		{
			GeometryRenderData data{};
			auto selectedCopy = sceneData.selectedObject;
			if (auto* t = selectedCopy.TryGetComponent<CTransform>())
				data.model = t->worldMatrix;
			if (auto* m = selectedCopy.TryGetComponent<CMesh>())
				data.geometry = m->mesh;
			outlinedGeometries.push_back(data);
		}
		mRendererFrontend->SetOutlinedGeometries(outlinedGeometries);
	}

	// Build world-space AABB cache for frustum culling.
	// Runs in EDITOR mode (always needed for bounding box overlays) and in any
	// mode when frustum culling is enabled. Without this, BuildRenderPacket would
	// find an empty cache and silently skip all culling in GAME mode.
	if (m_renderMode == RenderMode::EDITOR || frustumCullingEnabled)
	{
#ifdef _PROFILING
		ZoneScopedN("AABB Cache");
#endif
		mMeshAABBCache.clear();

		if (sceneData.registry)
		{
			std::vector<BoundingBoxData> boundingBoxes;
			auto view = sceneData.registry->view<CMesh, CTransform>();

			for (auto [entity, meshComp, transform] : view.each())
			{
				if (!meshComp.mesh) continue;
				if (meshComp.mesh->vertices.empty()) continue;

				const glm::vec3 localMin     = meshComp.mesh->localAABBMin;
				const glm::vec3 localMax     = meshComp.mesh->localAABBMax;
				const glm::vec3 localCenter  = (localMin + localMax) * 0.5f;
				const glm::vec3 localExtents = localMax - localMin;

				// ── OBB: apply full world transform (includes rotation) ────────────
				const glm::mat4& worldMatrix = transform.worldMatrix;
				glm::mat4 obbTransform = worldMatrix
					* glm::translate(glm::mat4(1.0f), localCenter)
					* glm::scale(glm::mat4(1.0f), localExtents);

				// ── AABB: compute world-space axis-aligned bounds ──────────────────
				const glm::vec3 corners[8] = {
					glm::vec3(worldMatrix * glm::vec4(localMin.x, localMin.y, localMin.z, 1.0f)),
					glm::vec3(worldMatrix * glm::vec4(localMax.x, localMin.y, localMin.z, 1.0f)),
					glm::vec3(worldMatrix * glm::vec4(localMin.x, localMax.y, localMin.z, 1.0f)),
					glm::vec3(worldMatrix * glm::vec4(localMax.x, localMax.y, localMin.z, 1.0f)),
					glm::vec3(worldMatrix * glm::vec4(localMin.x, localMin.y, localMax.z, 1.0f)),
					glm::vec3(worldMatrix * glm::vec4(localMax.x, localMin.y, localMax.z, 1.0f)),
					glm::vec3(worldMatrix * glm::vec4(localMin.x, localMax.y, localMax.z, 1.0f)),
					glm::vec3(worldMatrix * glm::vec4(localMax.x, localMax.y, localMax.z, 1.0f)),
				};

				glm::vec3 worldMin = corners[0];
				glm::vec3 worldMax = corners[0];
				for (const auto& c : corners)
				{
					worldMin = glm::min(worldMin, c);
					worldMax = glm::max(worldMax, c);
				}

				// Cache world-space AABB for frustum culling in BuildRenderPacket.
				const auto* info = sceneData.registry->try_get<CEntityInfo>(entity);
				const uint32_t id = info ? info->id : 0u;
				mMeshAABBCache[id] = { worldMin, worldMax };

				// Editor-only: generate OBB and AABB overlay geometry.
				if (m_renderMode == RenderMode::EDITOR)
				{
					const glm::vec3 worldCenter  = (worldMin + worldMax) * 0.5f;
					const glm::vec3 worldExtents = worldMax - worldMin;

					boundingBoxes.emplace_back(obbTransform, glm::vec4(0.3f, 0.6f, 1.0f, 1.0f)); // blue
					glm::mat4 aabbTransform = glm::translate(glm::mat4(1.0f), worldCenter)
						* glm::scale(glm::mat4(1.0f), worldExtents);
					boundingBoxes.emplace_back(aabbTransform, glm::vec4(1.0f, 0.4f, 0.1f, 1.0f)); // orange-red
				}
			}

			if (m_renderMode == RenderMode::EDITOR)
				mRendererFrontend->SetBoundingBoxes(boundingBoxes);
		}
	}
	else
	{
		mMeshAABBCache.clear();
	}

	// Editor-only: camera frustum overlays.
	if (m_renderMode == RenderMode::EDITOR)
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
				const float halfW_near  = halfH_near * cam.aspectRatio;
				const float halfH_far   = cam.farPlane  * halfTan;
				const float halfW_far   = halfH_far  * cam.aspectRatio;

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
	if (m_renderMode == RenderMode::EDITOR)
	{
#ifdef _PROFILING
		ZoneScopedN("Light Debugs");
#endif
		std::vector<BoundingBoxData> pointLightDebugs;

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
				if (sceneData.selectedObject.IsValid() &&
				    sceneData.selectedObject.GetEntity() == entity)
				{
					pointLightDebugs.emplace_back(
						glm::translate(glm::mat4(1.0f), transform.position) *
						glm::scale(glm::mat4(1.0f), glm::vec3(light.range)),
						color);
				}
			}
		}

		mRendererFrontend->SetPointLightDebugs(pointLightDebugs);
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

    // Destroy all GameObjects BEFORE freeing GPU resources.  Component
    // OnDestroy callbacks (CMesh, CMaterial) need the ResourceManager and
    // its Resource objects to still be alive so they can safely decrement
    // reference counts via UnloadResource().
    mModuleScene->ClearScene();

    // Destroy all GPU resources (textures, shaders, meshes, materials).
    // Safe because ReleaseFrameResources() already freed the CBs/FBs that
    // referenced these objects, and the scene has been cleared above so no
    // component still holds a reference to any Resource.
    mModuleResourceManager->ClearResources(mRendererFrontend);

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

void ModuleRenderer3D::WriteShaderManifest(const Resource* matShader, const Resource* bgShader) const
{
	JSON_Value*  root    = json_value_init_object();
	JSON_Object* rootObj = json_value_get_object(root);

	auto addEntry = [&](const char* key, const Resource* shader)
	{
		JSON_Value*  entry    = json_value_init_object();
		JSON_Object* entryObj = json_value_get_object(entry);
		json_object_set_number(entryObj, "uid",         static_cast<double>(shader->GetUID()));
		json_object_set_string(entryObj, "libraryPath", shader->GetLibraryPath().c_str());
		json_object_set_value(rootObj, key, entry);
	};

	addEntry("MaterialShader",   matShader);
	addEntry("BackgroundShader", bgShader);

	if (json_serialize_to_file_pretty(root, "Library/Shaders/shader_manifest.json") != JSONSuccess)
		NOUS_WARN_C(CURRENT_CHANNEL, "Failed to write Library/Shaders/shader_manifest.json.");
	else
		NOUS_INFO_C(CURRENT_CHANNEL, "Shader manifest written to Library/Shaders/shader_manifest.json.");

	json_value_free(root);
}

void ModuleRenderer3D::LoadShadersFromManifest()
{
	JSON_Value* root = json_parse_file("Library/Shaders/shader_manifest.json");
	if (!root)
	{
		NOUS_FATAL_C(CURRENT_CHANNEL,
			"Library/Shaders/shader_manifest.json not found. "
			"Run the editor once to generate it before packaging the game.");
		return;
	}

	const JSON_Object* rootObj = json_value_get_object(root);

	auto loadShader = [&](const char* key, const char* assetPath)
	{
		const JSON_Object* entry   = json_object_get_object(rootObj, key);
		if (!entry) { NOUS_ERROR_C(CURRENT_CHANNEL, "shader_manifest.json: missing entry '%s'.", key); return; }

		const uint32   uid     = static_cast<uint32>(json_object_get_number(entry, "uid"));
		const char* libPath = json_object_get_string(entry, "libraryPath");

		if (uid == 0 || !libPath)
		{ NOUS_ERROR_C(CURRENT_CHANNEL, "shader_manifest.json: invalid data for '%s'.", key); return; }

		mModuleResourceManager->CreateResourceFromLibrary(
			uid, ResourceType::SHADER, NOUS_FileManager::GetFilename(assetPath), assetPath, libPath);
	};

	loadShader("MaterialShader",   "Assets/Shaders/BuiltIn.MaterialShader.glsl");
	loadShader("BackgroundShader", "Assets/Shaders/BuiltIn.BackgroundShader.glsl");

	json_value_free(root);

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
	packet->hasDirectionalLight   = false;
	packet->activePointLightCount = 0;

	// Extract frustum planes from the game camera for per-mesh culling.
	// Meshes whose world-space AABB is completely outside the frustum are skipped.
	FrustumCulling::FrustumPlanes frustum{};
	bool hasFrustum = false;
	if (frustumCullingEnabled && packet->gameCamera)
	{
		const glm::mat4 vp = packet->gameCamera->GetProjectionMatrix() * packet->gameCamera->GetViewMatrix();
		frustum    = FrustumCulling::ExtractFrustumPlanes(vp);
		hasFrustum = true;
	}

	if (!sceneData.registry) return false;

	auto meshView = sceneData.registry->view<CMesh, CTransform>();
	packet->geometries.reserve(static_cast<size_t>(
		std::distance(meshView.begin(), meshView.end())));

	for (auto [entity, mesh, transform] : meshView.each())
	{
		if (!mesh.mesh) continue;

		GeometryRenderData data{};

		const auto* info = sceneData.registry->try_get<CEntityInfo>(entity);
		data.objectUID   = info ? info->id : 0u;
		data.model       = transform.worldMatrix;
		data.geometry    = mesh.mesh;

		if (const auto* mat = sceneData.registry->try_get<CMaterial>(entity))
			data.material = mat->material;

		// Frustum cull against the game camera using the cached world-space AABB.
		// Meshes with no cached AABB (e.g. empty vertex arrays) are not culled.
		if (hasFrustum)
		{
			const auto it = mMeshAABBCache.find(data.objectUID);
			if (it != mMeshAABBCache.end() &&
				!FrustumCulling::IsAABBVisible(frustum, it->second.first, it->second.second))
			{
				continue;
			}
		}

		packet->geometries.emplace_back(data);
	}

	// ── Light gathering ───────────────────────────────────────────────────────────
	auto lightView = sceneData.registry->view<CLight, CTransform>();
	for (auto [entity, light, transform] : lightView.each())
	{
		if (light.type == LightType::Directional)
		{
			if (!packet->hasDirectionalLight)
			{
				const glm::vec3 forward = glm::normalize(
					transform.orientation * glm::vec3(0.f, -1.f, 0.f));
				packet->directionalLight.direction = glm::vec4(forward, 0.f);
				packet->directionalLight.color     = glm::vec4(light.color, light.intensity);
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
			if (packet->activePointLightCount < c_maxPointLights)
			{
				PointLight& pl = packet->pointLights[packet->activePointLightCount++];
				pl.position    = glm::vec4(transform.position, light.range);
				pl.color       = glm::vec4(light.color, light.intensity);
			}
			else
			{
				const auto* info = sceneData.registry->try_get<CEntityInfo>(entity);
				NOUS_WARN_C(CURRENT_CHANNEL,
					"Point light limit (%u) reached; light on '%s' ignored.",
					c_maxPointLights, info ? info->name.c_str() : "<unknown>");
			}
		}
	}

	return true;
}
