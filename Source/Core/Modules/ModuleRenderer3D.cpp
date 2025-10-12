#include "ModuleRenderer3D.h"
#include "ModuleCamera3D.h"

#include "Renderer/Frontend/RendererFrontend.h"
#include "Renderer/RendererTypes.h"

#include "ECS/Scene.h"
#include "ECS/Components/ComponentMesh.h"
#include "ECS/Components/ComponentTransform.h"
#include "ECS/Components/ComponentMaterial.h"

#include "Systems/Memory Manager/MemoryManager.h"
#include "Utils/Logger.h"
#include "Systems/Event System/EventSystem.h"

#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif

// TODO: TEMP
#include "Systems/Texture System/TextureSystem.h"
#include "Systems/Material System/MaterialSystem.h"
#include "Systems/Geometry System/GeometrySystem.h"

ModuleRenderer3D::ModuleRenderer3D(Application* app) : Module(app)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	mRendererFrontend = NOUS_NEW<RendererFrontend>(MemoryManager::MemoryTag::RENDERER);
}

ModuleRenderer3D::~ModuleRenderer3D()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	NOUS_DELETE(mRendererFrontend, MemoryManager::MemoryTag::RENDERER);
}

bool ModuleRenderer3D::Awake()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	mRendererFrontend->SetBackendType(RendererBackendType::VULKAN);

	if (!mRendererFrontend->Initialize(mRendererFrontend->GetBackendType()))
	{
		NOUS_FATAL("[%s] Failed to initialize renderer frontend with backend of type (%d). Aborting application.",
				   __FUNCTION__, static_cast<int>(mRendererFrontend->GetBackendType()));
		return false;
	}

	return true;
}

bool ModuleRenderer3D::Start()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	// TODO: This should be done in a different way.
	NOUS_TextureSystem::Initialize();
	NOUS_MaterialSystem::Initialize();
	NOUS_GeometrySystem::Initialize();

	return true;
}

UpdateStatus ModuleRenderer3D::PreUpdate(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleRenderer3D::Update(float dt)
{
	NOUS_TRACE("%s()", __FUNCTION__);

	return UpdateStatus::CONTINUE;
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
	packet.gameCamera  = App->scene->gameCamera;

	if (BuildRenderPacket(&packet) && !App->isMinimized)
	{
		FrameResult result = mRendererFrontend->DrawFrame(&packet);

		switch (result)
		{
			case FrameResult::SUCCESS:
				break;

			case FrameResult::SKIPPED:
				NOUS_INFO("[%s] Frame skipped (window resize or swapchain recreation).", __FUNCTION__);
				break;

			case FrameResult::ERROR:
				NOUS_FATAL("[%s] Fatal rendering error. Aborting application.", __FUNCTION__);
				return UpdateStatus::ERROR;
		}
	}

	return UpdateStatus::CONTINUE;
}

bool ModuleRenderer3D::CleanUp()
{
	NOUS_TRACE("%s()", __FUNCTION__);

	// TODO: This should be done in a different way.
	NOUS_GeometrySystem::Shutdown();
	NOUS_MaterialSystem::Shutdown();
	NOUS_TextureSystem::Shutdown();

	mRendererFrontend->Shutdown();

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

			mRendererFrontend->OnResized(event.context._i64[0], event.context._i64[1]);

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
	return mRendererFrontend;
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
