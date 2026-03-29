#include "Engine/Renderer/Frontend/RendererFrontend.h"
#include "Engine/Renderer/Backend/RendererBackend.h"

#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"

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

void RendererFrontend::InjectDependencies(
    ModuleWindow* window,
    EventSystem* eventSystem,
    NOUS_Multithreading::NOUS_JobSystem* jobSystem,
    ModuleResourceManager* resourceManager)
{
    // Cache here — backend interface doesn't exist yet (Create() hasn't been called).
    // Applied in Initialize() after Create().
    m_window          = window;
    m_eventSystem     = eventSystem;
    m_jobSystem       = jobSystem;
    m_resourceManager = resourceManager;
}

bool RendererFrontend::Initialize(RendererBackendType backendType)
{
	mBackendType = backendType;

	if(!mBackend->Create(backendType))
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "Renderer backend failed to create. Aborting application...");
		return false;
	}

	// Backend interface now exists — forward cached dependencies into VulkanContext.
	mBackend->InjectDependencies(m_eventSystem, m_jobSystem, m_window, m_resourceManager);

	// Apply render mode now that the backend exists (created inside Create()).
	// This must happen before Initialize() which uses renderMode during setup.
	mBackend->SetRenderMode(mRenderMode);

	if (!mBackend->Initialize())
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "Renderer backend initialization failed. Aborting application...");

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
	if (!packet || !packet->gameCamera)
	{
		NOUS_WARN_C(CURRENT_CHANNEL, "Missing render packet or game camera; skipping frame.");
		return FrameResult::SKIPPED;
	}

	if (mRenderMode == RenderMode::EDITOR && !packet->editorCamera)
	{
		NOUS_WARN_C(CURRENT_CHANNEL, "Missing editor camera in EDITOR mode; skipping frame.");
		return FrameResult::SKIPPED;
	}

	// --- BEGIN FRAME ---
	const FrameResult beginResult = BeginFrame(packet->deltaTime);

	switch (beginResult)
	{
		case FrameResult::SUCCESS:
			break;

		case FrameResult::SKIPPED:
			NOUS_DEBUG_C(CURRENT_CHANNEL, "Frame skipped (swapchain recreation/minimized).");
			return FrameResult::SKIPPED;

		case FrameResult::ERROR:
			NOUS_ERROR_C(CURRENT_CHANNEL, "BeginFrame() failed.");
			return FrameResult::ERROR;
	}

	bool success = true;

	// --- SCENE PASS (EDITOR mode only) ---
	if (mRenderMode == RenderMode::EDITOR)
	{
		RenderpassType sceneRenderpass = RenderpassType::SCENE;
		success &= ExecuteRenderpass(sceneRenderpass, [&]()
		{
			success &= mBackend->DrawBackground(sceneRenderpass,
				packet->editorCamera->GetProjectionMatrix(),
				packet->editorCamera->GetViewMatrix());

			success &= mBackend->UpdateGlobalWorldState(
					sceneRenderpass,
					packet->editorCamera->GetProjectionMatrix(),
					packet->editorCamera->GetViewMatrix(),
					packet->editorCamera->GetPos(),
					glm::vec4(1.0f), 0);

			success &= mBackend->DrawGrid(
					sceneRenderpass,
					packet->editorCamera->GetProjectionMatrix(),
					packet->editorCamera->GetViewMatrix());

			for (auto& geometry : packet->geometries)
			{
				success &= mBackend->DrawGeometry(sceneRenderpass, geometry);
			}

			if (!mOutlinedGeometries.empty())
			{
				success &= mBackend->DrawOutlinedGeometries(
					sceneRenderpass,
					packet->editorCamera->GetProjectionMatrix(),
					packet->editorCamera->GetViewMatrix(),
					mOutlinedGeometries,
					mOutlineSettings);
			}

			if (!mBoundingBoxes.empty())
			{
				success &= mBackend->DrawBoundingBoxes(
					sceneRenderpass,
					packet->editorCamera->GetProjectionMatrix(),
					packet->editorCamera->GetViewMatrix(),
					mBoundingBoxes);
			}

			if (!mCameraFrustums.empty())
			{
				success &= mBackend->DrawCameraFrustums(
					sceneRenderpass,
					packet->editorCamera->GetProjectionMatrix(),
					packet->editorCamera->GetViewMatrix(),
					mCameraFrustums,
					/*globalAlreadySet=*/ !mBoundingBoxes.empty());
			}
		});
	}

	// --- GAME PASS ---
	RenderpassType gameRenderpass = RenderpassType::GAME;
	success &= ExecuteRenderpass(gameRenderpass, [&]()
	{
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

	// --- UI PASS (EDITOR mode only) ---
	if (mRenderMode == RenderMode::EDITOR)
	{
		RenderpassType uiRenderpass = RenderpassType::UI;
		success &= ExecuteRenderpass(uiRenderpass, [&]()
		{
			DrawEditor();
		});
	}

	// --- END FRAME ---
	const FrameResult endResult = EndFrame(packet->deltaTime);

	if (endResult == FrameResult::ERROR)
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "EndFrame() failed.");
		return FrameResult::ERROR;
	}

	if (endResult == FrameResult::SKIPPED)
	{
		NOUS_DEBUG_C(CURRENT_CHANNEL, "Frame skipped during EndFrame (likely swapchain reset).");
		return FrameResult::SKIPPED;
	}

	if (!success)
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "One or more render passes failed.");
		return FrameResult::ERROR;
	}

	return FrameResult::SUCCESS;
}

bool RendererFrontend::ExecuteRenderpass(RenderpassType type, const std::function<void()>& drawCommands)
{
	if (!mBackend->BeginRenderpass(type))
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "Begin Renderpass with type (%d) failed!", static_cast<int>(type));
		return false;
	}

	drawCommands();

	if (!mBackend->EndRenderpass(type))
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "End Renderpass with type (%d) failed!", static_cast<int>(type));
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

void RendererFrontend::ReloadAllShaders()
{
    if (!m_resourceManager)
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "[ShaderHotReload] No ResourceManager — cannot reload shaders.");
        return;
    }

    const auto resources = m_resourceManager->GetResourcesMap();
    int reloaded = 0;
    int failed   = 0;

    for (auto& [uid, resource] : resources)
    {
        if (resource->GetType() != ResourceType::SHADER)
            continue;

        auto* shader = static_cast<ResourceShader*>(resource);
        if (mBackend->ReloadShader(shader))
            ++reloaded;
        else
            ++failed;
    }

    if (failed == 0)
        NOUS_INFO_C(CURRENT_CHANNEL, "[ShaderHotReload] Reloaded %d shader(s) successfully.", reloaded);
    else
        NOUS_WARN_C(CURRENT_CHANNEL, "[ShaderHotReload] Reloaded %d shader(s); %d failed (see errors above).", reloaded, failed);
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

void RendererFrontend::SetBoundingBoxes(const std::vector<BoundingBoxData>& boxes)
{
	mBoundingBoxes = boxes;
}

void RendererFrontend::SetCameraFrustums(const std::vector<CameraFrustumData>& frustums)
{
	mCameraFrustums = frustums;
}

void RendererFrontend::SetEditorOverlay(IEditorOverlay *overlay)
{
    mEditorOverlay = overlay;
}

void RendererFrontend::SetRenderMode(RenderMode mode) noexcept
{
    // Stored locally; forwarded to VulkanContext inside Initialize() after backend Create().
    mRenderMode = mode;
}

RenderMode RendererFrontend::GetRenderMode() const noexcept
{
    return mRenderMode;
}

RendererBackendType RendererFrontend::GetBackendType() const noexcept
{
    return mBackendType;
}

void RendererFrontend::SetBackendType(RendererBackendType backendType) noexcept
{
    mBackendType = backendType;
}
