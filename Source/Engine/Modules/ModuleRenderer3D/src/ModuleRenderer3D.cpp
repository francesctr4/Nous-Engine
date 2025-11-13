#include "Engine/Modules/ModuleRenderer3D/include/ModuleRenderer3D.h"

#include "Engine/Core/Application.h"
#include "Engine/Modules/ModuleCamera3D/include/ModuleCamera3D.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"

#include "Engine/Renderer/Frontend/RendererFrontend.h"

#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/CMesh/include/CMesh.h"
#include "Engine/Systems/ECS/Component/CMaterial/include/CMaterial.h"
#include "Engine/Systems/ECS/Component/CTransform/include/CTransform.h"

#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/Core/Logger/LogChannel.h"
#include "Engine/Core/Logger/Logger.h"


#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_CORE_MODULE_RENDERER3D;

ModuleRenderer3D::ModuleRenderer3D(Application* app) : Module(app)
{
	NOUS_TRACE_C(CURRENT_CHANNEL, "%s()", __FUNCTION__);

	App->eventSystem->Subscribe(EventType::WINDOW_RESIZED, this);

	mRendererFrontend = NOUS_NEW<RendererFrontend>(MemoryTag::RENDERER);
}

ModuleRenderer3D::~ModuleRenderer3D()
{
	NOUS_TRACE_C(CURRENT_CHANNEL, "%s()", __FUNCTION__);

	NOUS_DELETE(mRendererFrontend, MemoryTag::RENDERER);
}

bool ModuleRenderer3D::Awake()
{
	NOUS_TRACE_C(CURRENT_CHANNEL, "%s()", __FUNCTION__);

	mRendererFrontend->SetBackendType(RendererBackendType::VULKAN);

	if (!mRendererFrontend->Initialize(mRendererFrontend->GetBackendType()))
	{
		NOUS_FATAL_C(CURRENT_CHANNEL, "[%s] Failed to initialize renderer frontend with backend of type (%d). Aborting application.",
				   __FUNCTION__, static_cast<int>(mRendererFrontend->GetBackendType()));
		return false;
	}

	return true;
}

bool ModuleRenderer3D::Start()
{
	NOUS_TRACE_C(CURRENT_CHANNEL, "%s()", __FUNCTION__);

	return true;
}

UpdateStatus ModuleRenderer3D::PreUpdate(float dt)
{
	NOUS_TRACE_C(CURRENT_CHANNEL, "%s()", __FUNCTION__);

	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleRenderer3D::Update(float dt)
{
	NOUS_TRACE_C(CURRENT_CHANNEL, "%s()", __FUNCTION__);

	return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleRenderer3D::PostUpdate(float dt)
{
	NOUS_TRACE_C(CURRENT_CHANNEL, "%s()", __FUNCTION__);

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
				NOUS_INFO_C(CURRENT_CHANNEL, "[%s] Frame skipped (window resize or swapchain recreation).", __FUNCTION__);
				break;

			case FrameResult::ERROR:
				NOUS_FATAL_C(CURRENT_CHANNEL, "[%s] Fatal rendering error. Aborting application.", __FUNCTION__);
				return UpdateStatus::ERROR;
		}
	}

	return UpdateStatus::CONTINUE;
}

bool ModuleRenderer3D::CleanUp()
{
	NOUS_TRACE_C(CURRENT_CHANNEL, "%s()", __FUNCTION__);

    App->resourceManager->ClearResources();

	mRendererFrontend->Shutdown();

	NOUS_INFO_C(CURRENT_CHANNEL, "[%s] Renderer Frontend shutdown was successful.", __FUNCTION__);

	return true;
}

void ModuleRenderer3D::OnEvent(const Event& event)
{
	switch (event.type)
	{
		case EventType::WINDOW_RESIZED:
		{
			NOUS_INFO_C(CURRENT_CHANNEL, "[%s] Event Received: WINDOW_RESIZED (%d) - Context: %d, %d",
					  __FUNCTION__,
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
		NOUS_ERROR_C(CURRENT_CHANNEL, "[%s] Active scene is not defined. Render packet will not be built.", __FUNCTION__);
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
