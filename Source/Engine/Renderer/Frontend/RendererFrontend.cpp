#include "Engine/Renderer/Frontend/RendererFrontend.h"
#include "Engine/Renderer/Backend/RendererBackend.h"

#include "Engine/Systems/CameraSystem/Camera/include/Camera.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Renderer/Frontend/IEditorOverlay.h"

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_RENDERER_FRONTEND;

RendererFrontend::RendererFrontend()
{
	mBackendType = RendererBackendType::UNKNOWN;
	mBackend = NOUS_NEW<RendererBackend>(MemoryTag::RENDERER);

    mEditorOverlay = nullptr;
}

RendererFrontend::~RendererFrontend()
{
	NOUS_DELETE(mBackend, MemoryTag::RENDERER);
}

bool RendererFrontend::Initialize(RendererBackendType backendType)
{
	mBackendType = backendType;

	if(!mBackend->Create(backendType))
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "[%s] Renderer backend failed to create. Aborting application...", __FUNCTION__);
		return false;
	}

	if (!mBackend->Initialize())
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "[%s] Renderer backend initialization failed. Aborting application...", __FUNCTION__);

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

void RendererFrontend::ReleaseFrameResources() noexcept
{
    if (mBackend)
        mBackend->ReleaseFrameResources();
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
		mBackend->mFrameNumber++;
	}

	return result;
}

FrameResult RendererFrontend::DrawFrame(RenderPacket* packet)
{
	if (!packet || !packet->editorCamera || !packet->gameCamera)
	{
		NOUS_WARN_C(CURRENT_CHANNEL, "[%s] Missing render packet or cameras; skipping frame.", __FUNCTION__);
		return FrameResult::SKIPPED;
	}

	// --- BEGIN FRAME ---
	const FrameResult beginResult = BeginFrame(packet->deltaTime);

	switch (beginResult)
	{
		case FrameResult::SUCCESS:
			break;

		case FrameResult::SKIPPED:
			NOUS_DEBUG_C(CURRENT_CHANNEL, "[%s] Frame skipped (swapchain recreation/minimized).", __FUNCTION__);
			return FrameResult::SKIPPED;

		case FrameResult::ERROR:
			NOUS_ERROR_C(CURRENT_CHANNEL, "[%s] BeginFrame() failed.", __FUNCTION__);
			return FrameResult::ERROR;
	}

	bool success = true;

	// --- SCENE PASS ---
	RenderpassType sceneRenderpass = RenderpassType::SCENE;
	success &= ExecuteRenderpass(sceneRenderpass, [&]()
	{
		// Background gradient must come first — before geometry and the grid.
		success &= mBackend->DrawBackground(sceneRenderpass,
			packet->editorCamera->GetProjectionMatrix(),
			packet->editorCamera->GetViewMatrix());

        success &= mBackend->UpdateGlobalWorldState(
				sceneRenderpass,
				packet->editorCamera->GetProjectionMatrix(),
				packet->editorCamera->GetViewMatrix(),
				packet->editorCamera->GetPos(),
				glm::vec4(1.0f), 0);

		// Draw the editor reference grid at the world origin (XZ plane).
		success &= mBackend->DrawGrid(
				sceneRenderpass,
				packet->editorCamera->GetProjectionMatrix(),
				packet->editorCamera->GetViewMatrix());

		for (auto& geometry : packet->geometries)
		{
            success &= mBackend->DrawGeometry(sceneRenderpass, geometry);
		}

		// Stencil-based outline pass (scene viewport only).
		if (!mOutlinedGeometries.empty())
		{
			success &= mBackend->DrawOutlinedGeometries(
				sceneRenderpass,
				packet->editorCamera->GetProjectionMatrix(),
				packet->editorCamera->GetViewMatrix(),
				mOutlinedGeometries,
				mOutlineSettings);
		}
	});

	// --- GAME PASS ---
	RenderpassType gameRenderpass = RenderpassType::GAME;
	success &= ExecuteRenderpass(gameRenderpass, [&]()
	{
		// Background gradient must come first — before geometry.
		success &= mBackend->DrawBackground(gameRenderpass,
			packet->gameCamera->GetProjectionMatrix(),
			packet->gameCamera->GetViewMatrix());

        success &= mBackend->UpdateGlobalWorldState(
				gameRenderpass,
				packet->gameCamera->GetProjectionMatrix(),
				packet->gameCamera->GetViewMatrix(),
				packet->gameCamera->GetPos(),
				glm::vec4(1.0f), 0);

		for (auto& geometry : packet->geometries)
		{
            success &= mBackend->DrawGeometry(gameRenderpass, geometry);
		}
	});

	// --- UI PASS ---
	RenderpassType uiRenderpass = RenderpassType::UI;
	success &= ExecuteRenderpass(uiRenderpass, [&]()
	{
		DrawEditor();
	});

	// --- END FRAME ---
	const FrameResult endResult = EndFrame(packet->deltaTime);

	if (endResult == FrameResult::ERROR)
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "[%s] EndFrame() failed.", __FUNCTION__);
		return FrameResult::ERROR;
	}

	if (endResult == FrameResult::SKIPPED)
	{
		NOUS_DEBUG_C(CURRENT_CHANNEL, "[%s] Frame skipped during EndFrame (likely swapchain reset).", __FUNCTION__);
		return FrameResult::SKIPPED;
	}

	if (!success)
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "[%s] One or more render passes failed.", __FUNCTION__);
		return FrameResult::ERROR;
	}

	return FrameResult::SUCCESS;
}

bool RendererFrontend::ExecuteRenderpass(RenderpassType type, const std::function<void()>& drawCommands)
{
	if (!mBackend->BeginRenderpass(type))
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "[%s] Begin Renderpass with type (%d) failed!", __FUNCTION__, static_cast<int>(type));
		return false;
	}

	drawCommands();

	if (!mBackend->EndRenderpass(type))
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "[%s] End Renderpass with type (%d) failed!", __FUNCTION__, static_cast<int>(type));
		return false;
	}

	return true;
}

void RendererFrontend::DrawEditor()
{
    if (mEditorOverlay)
    {
        mEditorOverlay->DrawEditor();
    }
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

bool RendererFrontend::CreateShader(ResourceShader* shader)
{
	return mBackend->CreateShader(shader);
}

void RendererFrontend::DestroyShader(ResourceShader* shader)
{
	mBackend->DestroyShader(shader);
}

uint32_t RendererFrontend::PickObjectAt(int32_t pixelX, int32_t pixelY,
                                        const glm::mat4& projection, const glm::mat4& view,
                                        const std::vector<GeometryRenderData>& geometries)
{
    if (!mBackend || geometries.empty())
        return 0;

    return mBackend->PickObjectAt(pixelX, pixelY, projection, view, geometries);
}

bool RendererFrontend::SetOutlinedGeometries(
	const std::vector<GeometryRenderData>& selectedGeometries,
	const OutlineSettings& outlineSettings)
{
	mOutlinedGeometries = selectedGeometries;
	mOutlineSettings    = outlineSettings;
	return true;
}

void RendererFrontend::SetEditorOverlay(IEditorOverlay *overlay)
{
    mEditorOverlay = overlay;
}

RendererBackendType RendererFrontend::GetBackendType() const noexcept
{
    return mBackendType;
}

void RendererFrontend::SetBackendType(RendererBackendType backendType) noexcept
{
    mBackendType = backendType;
}
