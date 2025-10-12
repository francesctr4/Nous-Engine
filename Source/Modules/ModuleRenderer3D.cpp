#include "Modules/ModuleRenderer3D.h"

#include "Modules/ModuleCamera3D.h"
#include "Modules/ModuleScene.h"

#include "Renderer/Frontend/RendererFrontend.h"
#include "Renderer/RendererTypes.h"

#include "ECS/Scene.h"
#include "ECS/Components/ComponentMesh.h"
#include "ECS/Components/ComponentTransform.h"
#include "ECS/Components/ComponentMaterial.h"

#include "Systems/Memory Manager/MemoryManager.h"
#include "Utils/Logger.h"

#ifdef _PROFILING
#include "Includes/Tracy.h"
#endif

ModuleRenderer3D::ModuleRenderer3D(Application* app) : Module(app)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	rendererFrontend = NOUS_NEW<RendererFrontend>(MemoryManager::MemoryTag::RENDERER);
}

ModuleRenderer3D::~ModuleRenderer3D()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	NOUS_DELETE(rendererFrontend, MemoryManager::MemoryTag::RENDERER);
}

bool ModuleRenderer3D::Awake()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	rendererFrontend->backendType = RendererBackendType::VULKAN;

	if (!rendererFrontend->Initialize(rendererFrontend->backendType))
	{
		NOUS_FATAL("[%s] Failed to initialize renderer. Aborting application.", __FUNCTION__);
		return false;
	}

	NOUS_INFO("[%s] Renderer Frontend initialized successfully with Renderer Backend: %d",
			  __FUNCTION__, static_cast<int>(rendererFrontend->backendType));

	return true;
}

bool ModuleRenderer3D::Start()
{
	NOUS_TRACE("%s()", __FUNCTION__);
	
	return true;
}

UpdateStatus ModuleRenderer3D::PreUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return UPDATE_CONTINUE;
}

UpdateStatus ModuleRenderer3D::Update(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return UPDATE_CONTINUE;
}

UpdateStatus ModuleRenderer3D::PostUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

#ifdef _PROFILING
	ZoneScoped;
#endif

	RenderPacket packet{};

	packet.deltaTime = dt;
	packet.editorCamera = App->camera->GetCamera();
	packet.gameCamera = App->scene->gameCamera;

	if (BuildRenderPacket(&packet) && !App->isMinimized)
	{
		if (!rendererFrontend->DrawFrame(&packet))
		{
			NOUS_FATAL("[%s] Failed to draw frame. Aborting application.", __FUNCTION__);
			return UPDATE_ERROR;
		}
	}

	return UPDATE_CONTINUE;
}

bool ModuleRenderer3D::CleanUp()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	if (!rendererFrontend->Shutdown())
	{
		NOUS_FATAL("[%s] Failed to shutdown renderer. Aborting application.", __FUNCTION__);
		return false;
	}

	NOUS_INFO("[%s] Renderer Frontend shutdown was successful.", __FUNCTION__);
	return true;
}

void ModuleRenderer3D::ReceiveEvent(const Event& event)
{
	switch (event.type)
	{
		case EventType::WINDOW_RESIZED:
		{
			NOUS_INFO("[%s] Event Received: WINDOW_RESIZED (%d) - Context: %d, %d",
					  __FUNCTION__,
					  static_cast<int>(event.type),
					  event.context._i64[0],
					  event.context._i64[1]);

			rendererFrontend->OnResized(event.context._i64[0], event.context._i64[1]);

			break;
		}
		default: 
		{
			NOUS_WARN("[%s] Default case. Unhandled event received! (%d)",
					  __FUNCTION__,
					  static_cast<int>(event.type));

			break;
		}
	}
}

RendererFrontend *ModuleRenderer3D::GetRendererFrontend() const
{
	return rendererFrontend;
}

bool ModuleRenderer3D::BuildRenderPacket(RenderPacket* packet)
{
	if (!App->scene->activeScene)
	{
		NOUS_ERROR("[%s] Active scene is not defined. Render packet will not be built.", __FUNCTION__);
		return false;
	}

	packet->geometries.clear();

	const auto& gameObjects = App->scene->activeScene->GetGameObjects();
	packet->geometries.reserve(gameObjects.size());

	for (const auto& goPtr : gameObjects)
	{
		GameObject* go = goPtr.get();
		if (!go->HasComponent<CMesh>()) continue;

		GeometryRenderData data{};

		if (auto* transform = go->TryGetComponent<CTransform>())
			data.model = transform->worldMatrix;

		if (auto* mesh = go->TryGetComponent<CMesh>())
			data.geometry = mesh->mesh;

		if (auto* material = go->TryGetComponent<CMaterial>())
			data.material = material->material;

		packet->geometries.emplace_back(data);
	}

	return true;
}
