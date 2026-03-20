#include "Engine/Modules/ModuleRenderer3D/include/ModuleRenderer3D.h"

#include "Engine/Core/Application.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include "Engine/Modules/ModuleCamera3D/include/ModuleCamera3D.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"

#include "Engine/Renderer/Frontend/RendererFrontend.h"

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"
#include "Engine/Systems/ECS/Component/CCamera/include/CCamera.h"

#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/Core/Logger/LogChannel.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Systems/CameraSystem/Camera/include/Camera.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMesh/include/ResourceMesh.h"
#include "Engine/Utils/Math/FrustumCulling.h"


#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MODULE_RENDERER3D;

ModuleRenderer3D::ModuleRenderer3D(Application* app) : Module(app)
{
	App->eventSystem->Subscribe(EventType::WINDOW_RESIZED, this);

	mRendererFrontend = NOUS_NEW<RendererFrontend>(MemoryTag::RENDERER);
}

ModuleRenderer3D::~ModuleRenderer3D()
{
	NOUS_DELETE(mRendererFrontend, MemoryTag::RENDERER);
}

bool ModuleRenderer3D::Awake()
{
	mRendererFrontend->SetBackendType(RendererBackendType::VULKAN);

	if (!mRendererFrontend->Initialize(mRendererFrontend->GetBackendType()))
	{
		NOUS_FATAL_C(CURRENT_CHANNEL, "Failed to initialize renderer frontend with backend of type (%d). Aborting application.",
				   static_cast<int>(mRendererFrontend->GetBackendType()));
		return false;
	}

	// ------------------------------ SHADERS ------------------------------ //

	// Load BuiltIn shaders now that the Vulkan backend and ResourceManager are both ready.
	// This guarantees the shaders exist before any Start() call or rendering begins.
	App->resourceManager->CreateResource("Assets/Shaders/BuiltIn.MaterialShader.glsl");
	App->resourceManager->CreateResource("Assets/Shaders/BuiltIn.PickShader.glsl");
	App->resourceManager->CreateResource("Assets/Shaders/BuiltIn.OutlineShader.glsl");
	App->resourceManager->CreateResource("Assets/Shaders/BuiltIn.GridShader.glsl");
	App->resourceManager->CreateResource("Assets/Shaders/BuiltIn.BackgroundShader.glsl");
	App->resourceManager->CreateResource("Assets/Shaders/BuiltIn.BoundingBoxShader.glsl");

	// TEMP SHADERS (DEBUG)
	//App->resourceManager->CreateResource("Assets/Shaders/temp_MockShader.glsl");
	//App->resourceManager->CreateResource("Assets/Shaders/temp_ShaderWithAllStages.glsl");

	return true;
}

bool ModuleRenderer3D::Start()
{
	return true;
}

UpdateStatus ModuleRenderer3D::PreUpdate(float dt)
{
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

	RenderPacket packet{};
	packet.deltaTime = dt;
	packet.editorCamera = App->camera->GetCamera();
	packet.gameCamera  = App->scene->gameCamera;

	// Populate the outline list from the currently selected GameObject.
	{
		std::vector<GeometryRenderData> outlinedGeometries;
		if (App->scene->selectedGameObject && App->scene->selectedGameObject->HasComponent<CMesh>())
		{
			GeometryRenderData data{};
			if (auto* t = App->scene->selectedGameObject->TryGetComponent<CTransform>())
				data.model = t->worldMatrix;
			if (auto* m = App->scene->selectedGameObject->TryGetComponent<CMesh>())
				data.geometry = m->mesh;
			outlinedGeometries.push_back(data);
		}
		mRendererFrontend->SetOutlinedGeometries(outlinedGeometries);
	}

	// Compute AABB and OBB bounding boxes for all GameObjects with meshes.
	// The world-space AABBs are cached in mMeshAABBCache for use by BuildRenderPacket.
	{
		mMeshAABBCache.clear();
		std::vector<BoundingBoxData> boundingBoxes;

		if (App->scene->activeScene)
		{
		const auto gameObjects = App->scene->activeScene->GetGameObjectsSnapshot();

		for (const auto& goPtr : gameObjects)
		{
			auto* meshComp = goPtr->TryGetComponent<CMesh>();
			auto* transform = goPtr->TryGetComponent<CTransform>();
			if (!meshComp || !meshComp->mesh || !transform)
				continue;

			const auto& vertices = meshComp->mesh->vertices;
			if (vertices.empty())
				continue;

			// Compute local AABB from mesh vertices.
			glm::vec3 localMin = vertices[0].position;
			glm::vec3 localMax = vertices[0].position;
			for (const auto& v : vertices)
			{
				localMin = glm::min(localMin, v.position);
				localMax = glm::max(localMax, v.position);
			}

			const glm::vec3 localCenter  = (localMin + localMax) * 0.5f;
			const glm::vec3 localExtents = localMax - localMin;

			// ── OBB: apply full world transform (includes rotation) ────────────
			// transform = worldMatrix * translate(localCenter) * scale(localExtents)
			const glm::mat4& worldMatrix = transform->worldMatrix;
			glm::mat4 obbTransform = worldMatrix
				* glm::translate(glm::mat4(1.0f), localCenter)
				* glm::scale(glm::mat4(1.0f), localExtents);
			boundingBoxes.emplace_back(obbTransform, glm::vec4(0.3f, 0.6f, 1.0f, 1.0f)); // blue

			// ── AABB: compute world-space axis-aligned bounds ──────────────────
			// Transform all 8 local corners through the world matrix, then take min/max.
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

			const glm::vec3 worldCenter  = (worldMin + worldMax) * 0.5f;
			const glm::vec3 worldExtents = worldMax - worldMin;

			glm::mat4 aabbTransform = glm::translate(glm::mat4(1.0f), worldCenter)
				* glm::scale(glm::mat4(1.0f), worldExtents);
			boundingBoxes.emplace_back(aabbTransform, glm::vec4(1.0f, 0.4f, 0.1f, 1.0f)); // orange-red

			// Cache for frustum culling in BuildRenderPacket.
			mMeshAABBCache[goPtr->GetID()] = { worldMin, worldMax };
		}
		} // if activeScene

		mRendererFrontend->SetBoundingBoxes(boundingBoxes);
	}

	// Collect camera frustums from all GameObjects with a CCamera component.
	{
		std::vector<CameraFrustumData> frustums;

		if (App->scene->activeScene)
		{
			const auto gameObjects = App->scene->activeScene->GetGameObjectsSnapshot();

			for (const auto& goPtr : gameObjects)
			{
				auto* cam       = goPtr->TryGetComponent<CCamera>();
				auto* transform = goPtr->TryGetComponent<CTransform>();
				if (!cam || !transform)
					continue;

				const float vfovRad     = glm::radians(cam->fov);
				const float halfTan     = std::tan(vfovRad * 0.5f);
				const float halfH_near  = cam->nearPlane * halfTan;
				const float halfW_near  = halfH_near * cam->aspectRatio;
				const float halfH_far   = cam->farPlane  * halfTan;
				const float halfW_far   = halfH_far  * cam->aspectRatio;

				const glm::vec3 pos     = transform->position;
				const glm::vec3 fwd     = transform->GetForward();
				const glm::vec3 up      = transform->GetUp();
				const glm::vec3 right   = transform->GetRight();

				const glm::vec3 nearCenter = pos + fwd * cam->nearPlane;
				const glm::vec3 farCenter  = pos + fwd * cam->farPlane;

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
				fdata.color = cam->isMainCamera
					? glm::vec4(1.0f, 0.85f, 0.0f, 1.0f)
					: glm::vec4(0.2f, 0.9f,  0.2f, 1.0f);

				frustums.push_back(fdata);
			}
		}

		mRendererFrontend->SetCameraFrustums(frustums);
	}

	if (BuildRenderPacket(&packet) && !App->isMinimized)
	{
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
    App->scene->selectedGameObject = nullptr;
    if (App->scene->activeScene)
        App->scene->activeScene->Clear();

    // Destroy all GPU resources (textures, shaders, meshes, materials).
    // Safe because ReleaseFrameResources() already freed the CBs/FBs that
    // referenced these objects, and the scene has been cleared above so no
    // component still holds a reference to any Resource.
    App->resourceManager->ClearResources();

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
        case EventType::NONE:
        case EventType::TEST:
        case EventType::KEY_PRESSED:
        case EventType::SWAP_TEXTURE:
        case EventType::DROP_FILE:
        case EventType::INPUT_EVENT:
        case EventType::IMGUI_RECREATION:
        case EventType::WINDOW_MINIMIZED:
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

RendererFrontend *ModuleRenderer3D::GetRendererFrontend() const
{
	return mRendererFrontend;
}

bool ModuleRenderer3D::BuildRenderPacket(RenderPacket* packet)
{
	if (!App->scene->activeScene)
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "Active scene is not defined. Render packet will not be built.");
		return false;
	}

	packet->geometries.clear();

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

	// Snapshot under mutex — guards against concurrent CreateGameObject() calls
	// from the background LoadScene job reallocating the vector mid-iteration.
	const auto gameObjects = App->scene->activeScene->GetGameObjectsSnapshot();
	packet->geometries.reserve(gameObjects.size());

	for (const auto& goPtr : gameObjects)
	{
		if (!goPtr->HasComponent<CMesh>()) continue;

		GeometryRenderData data{};

		data.objectUID = goPtr->GetID();

		if (auto* transform = goPtr->TryGetComponent<CTransform>())
			data.model = transform->worldMatrix;

		if (auto* mesh = goPtr->TryGetComponent<CMesh>())
			data.geometry = mesh->mesh;

		if (auto* material = goPtr->TryGetComponent<CMaterial>())
			data.material = material->material;

		// Frustum cull against the game camera using the cached world-space AABB.
		// Meshes with no cached AABB (e.g. empty vertex arrays) are not culled.
		if (hasFrustum)
		{
			const auto it = mMeshAABBCache.find(goPtr->GetID());
			if (it != mMeshAABBCache.end() &&
				!FrustumCulling::IsAABBVisible(frustum, it->second.first, it->second.second))
			{
				continue;
			}
		}

		packet->geometries.emplace_back(data);
	}

	return true;
}
