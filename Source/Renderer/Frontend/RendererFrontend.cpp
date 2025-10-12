#include "Renderer/Frontend/RendererFrontend.h"
#include "Renderer/Backend/RendererBackend.h"

#include "Core/Application.h"
#include "Core/Modules/ModuleEditor.h"

#include "Systems/Camera System/Camera.h"
#include "Systems/Memory Manager/MemoryManager.h"
#include "Utils/Logger.h"

RendererFrontend::RendererFrontend()
{
	mBackendType = RendererBackendType::UNKNOWN;
	mBackend = NOUS_NEW<RendererBackend>(MemoryManager::MemoryTag::RENDERER);
}

RendererFrontend::~RendererFrontend()
{
	NOUS_DELETE(mBackend, MemoryManager::MemoryTag::RENDERER);
}

bool RendererFrontend::Initialize(RendererBackendType backendType)
{
	mBackendType = backendType;

	if(!mBackend->Create(backendType))
	{
		NOUS_ERROR("[%s] Renderer backend failed to create. Aborting application...", __FUNCTION__);
		return false;
	}

	if (!mBackend->Initialize())
	{
		NOUS_ERROR("[%s] Renderer backend initialization failed. Aborting application...", __FUNCTION__);

		Shutdown();
		return false;
	}

	return true;
}

void RendererFrontend::Shutdown()
{
	if (!mBackend) return;

	mBackend->Shutdown();
	mBackend->Destroy();
}

void RendererFrontend::OnResized(uint16_t width, uint16_t height)
{
	mBackend->Resized(width, height);
}

FrameResult RendererFrontend::BeginFrame(float dt)
{
	return mBackend->BeginFrame(dt);
}

FrameResult RendererFrontend::EndFrame(float dt)
{
	FrameResult result = mBackend->EndFrame(dt);

	if (result == FrameResult::SUCCESS)
	{
		mBackend->frameNumber++;
	}

	return result;
}

FrameResult RendererFrontend::DrawFrame(RenderPacket* packet)
{
	if (!packet || !packet->editorCamera || !packet->gameCamera)
	{
		NOUS_WARN("[%s] Missing render packet or cameras; skipping frame.", __FUNCTION__);
		return FrameResult::SKIPPED;
	}

	// --- BEGIN FRAME ---
	const FrameResult beginResult = BeginFrame(packet->deltaTime);

	switch (beginResult)
	{
		case FrameResult::SUCCESS:
			break;

		case FrameResult::SKIPPED:
			NOUS_DEBUG("[%s] Frame skipped (swapchain recreation/minimized).", __FUNCTION__);
			return FrameResult::SKIPPED;

		case FrameResult::ERROR:
			NOUS_ERROR("[%s] BeginFrame() failed.", __FUNCTION__);
			return FrameResult::ERROR;
	}

	bool success = true;

	// --- SCENE PASS ---
	RenderpassType sceneRenderpass = RenderpassType::SCENE;
	success &= ExecuteRenderpass(sceneRenderpass, [&]()
	{
		mBackend->UpdateGlobalWorldState(
				sceneRenderpass,
				packet->editorCamera->GetProjectionMatrix(),
				packet->editorCamera->GetViewMatrix(),
				packet->editorCamera->GetPos(),
				glm::vec4(1.0f), 0);

		for (auto& geometry : packet->geometries)
		{
			mBackend->DrawGeometry(sceneRenderpass, geometry);
		}
	});

	// --- GAME PASS ---
	RenderpassType gameRenderpass = RenderpassType::GAME;
	success &= ExecuteRenderpass(gameRenderpass, [&]()
	{
		mBackend->UpdateGlobalWorldState(
				gameRenderpass,
				packet->gameCamera->GetProjectionMatrix(),
				packet->gameCamera->GetViewMatrix(),
				packet->gameCamera->GetPos(),
				glm::vec4(1.0f), 0);

		for (auto& geometry : packet->geometries)
		{
			mBackend->DrawGeometry(gameRenderpass, geometry);
		}
	});

	// --- UI PASS ---
	RenderpassType uiRenderpass = RenderpassType::UI;
	success &= ExecuteRenderpass(uiRenderpass, [&]()
	{
		mBackend->UpdateGlobalUIState(
				uiRenderpass,
				packet->editorCamera->GetProjectionMatrix(),
				packet->editorCamera->GetViewMatrix(), 0);

		DrawEditor();
	});

	// --- END FRAME ---
	const FrameResult endResult = EndFrame(packet->deltaTime);

	if (endResult == FrameResult::ERROR)
	{
		NOUS_ERROR("[%s] EndFrame() failed.", __FUNCTION__);
		return FrameResult::ERROR;
	}

	if (endResult == FrameResult::SKIPPED)
	{
		NOUS_DEBUG("[%s] Frame skipped during EndFrame (likely swapchain reset).", __FUNCTION__);
		return FrameResult::SKIPPED;
	}

	if (!success)
	{
		NOUS_ERROR("[%s] One or more render passes failed.", __FUNCTION__);
		return FrameResult::ERROR;
	}

	return FrameResult::SUCCESS;
}

bool RendererFrontend::ExecuteRenderpass(RenderpassType type, const std::function<void()>& drawCommands)
{
	if (!mBackend->BeginRenderpass(type))
	{
		NOUS_ERROR("[%s] Begin Renderpass with type (%d) failed!", __FUNCTION__, static_cast<int>(type));
		return false;
	}

	drawCommands();

	if (!mBackend->EndRenderpass(type))
	{
		NOUS_ERROR("[%s] End Renderpass with type (%d) failed!", __FUNCTION__, static_cast<int>(type));
		return false;
	}

	return true;
}

void RendererFrontend::DrawEditor()
{
	External->editor->DrawEditor();
}

bool RendererFrontend::CreateTexture(const uint8_t* pixels, ResourceTexture* outTexture)
{
	return mBackend->CreateTexture(pixels, outTexture);
}

void RendererFrontend::DestroyTexture(ResourceTexture* texture)
{
	mBackend->DestroyTexture(texture);
}

bool RendererFrontend::CreateMaterial(ResourceMaterial* material)
{
	return mBackend->CreateMaterial(material);
}

void RendererFrontend::DestroyMaterial(ResourceMaterial* material)
{
	mBackend->DestroyMaterial(material);
}

bool RendererFrontend::CreateGeometry(uint32_t vertexCount, const Vertex3D* vertices, uint32_t indexCount, const uint32_t* indices, ResourceMesh* outGeometry)
{
	return mBackend->CreateGeometry(vertexCount, vertices, indexCount, indices, outGeometry);
}

void RendererFrontend::DestroyGeometry(ResourceMesh* geometry)
{
	mBackend->DestroyGeometry(geometry);
}
