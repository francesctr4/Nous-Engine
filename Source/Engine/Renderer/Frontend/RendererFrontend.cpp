#include "Engine/Renderer/Frontend/RendererFrontend.h"
#include "Engine/Renderer/Backend/RendererBackend.h"

#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"
#include "Engine/Systems/ShaderSystem/ShaderLoader/include/ShaderLoader.h"
#include "Engine/Systems/ShaderSystem/ShaderCompiler/include/ShaderCompilerTypes.h"
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"

#include <filesystem>

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

	// Wait for any in-flight compile jobs to finish before tearing down GPU resources.
	// Jobs hold a ResourceShader* and push to m_readySwaps — both must remain valid.
	while (m_inFlightJobCount.load(std::memory_order_acquire) > 0) { /* spin */ }

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
    // Defer the actual work to FlushPendingReloads(), which is called from
    // ModuleRenderer3D::PreUpdate() — safely between frames, before BeginFrame().
    // This method may be called from inside a renderpass (e.g. UI menu callback).
    m_pendingReloadAll = true;
}

void RendererFrontend::FlushPendingReloads()
{
    if (!m_pendingReloadAll)
        return;

    m_pendingReloadAll = false;

    if (!m_resourceManager || !m_jobSystem)
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "[ShaderHotReload] No ResourceManager/JobSystem — cannot dispatch compile jobs.");
        return;
    }

    for (auto& [uid, resource] : m_resourceManager->GetResourcesMap())
    {
        if (resource->GetType() != ResourceType::SHADER)
            continue;

        auto* shader = static_cast<ResourceShader*>(resource);
        const std::string normalized =
            std::filesystem::path(shader->GetAssetsPath()).generic_string();
        DispatchCompileJob(normalized, shader);
    }
}

void RendererFrontend::FlushCompletedReloads()
{
    // Drain the ready queue under the lock, then release it before doing GPU work.
    std::vector<PendingGPUSwap> toApply;
    {
        std::lock_guard<std::mutex> lock(m_swapQueueMutex);
        if (m_readySwaps.empty()) return;
        toApply = std::move(m_readySwaps);
        m_readySwaps.clear();
    }

    int applied = 0;
    int failed  = 0;

    for (auto& pending : toApply)
    {
        ResourceShader* shader = pending.shader;
        ShaderLoadResult& result = pending.compileResult;

        // Move compiled CPU data into the live shader and free the temporary one.
        shader->stagesData = std::move(result.shader->stagesData);
        shader->reflection = std::move(result.shader->reflection);
        shader->generation++;
        NOUS_DELETE(result.shader, MemoryTag::RESOURCE_SHADER);

        // GPU swap (vkDeviceWaitIdle + Destroy + Create).
        if (mBackend->ApplyCompiledShader(shader))
            ++applied;
        else
        {
            ++failed;
            NOUS_WARN_C(CURRENT_CHANNEL, "[ShaderHotReload] GPU swap failed for '%s'.",
                        shader->GetAssetsPath().c_str());
        }
    }

    if (applied > 0 || failed > 0)
    {
        if (failed == 0)
            NOUS_INFO_C(CURRENT_CHANNEL, "[ShaderHotReload] Applied %d compiled shader(s).", applied);
        else
            NOUS_WARN_C(CURRENT_CHANNEL, "[ShaderHotReload] Applied %d shader(s); %d GPU swap(s) failed.", applied, failed);
    }
}

void RendererFrontend::DispatchCompileJob(const std::string& path, ResourceShader* shader)
{
    {
        std::lock_guard<std::mutex> lock(m_swapQueueMutex);
        if (m_inFlightPaths.count(path))
            return;  // compile already in-flight for this path — skip duplicate
        m_inFlightPaths.insert(path);
    }

    m_inFlightJobCount.fetch_add(1, std::memory_order_relaxed);

    m_jobSystem->SubmitJob([this, path, shader]()
    {
        NOUS_INFO_C(CURRENT_CHANNEL, "[ShaderHotReload] Compiling '%s'...", path.c_str());

        const ShaderCompilerConfig config;
        ShaderLoadResult result = NOUS_ShaderSystem::LoadShaderFromFile(path, config);

        {
            std::lock_guard<std::mutex> lock(m_swapQueueMutex);
            m_inFlightPaths.erase(path);

            if (result.success)
            {
                m_readySwaps.push_back({ shader, std::move(result) });
            }
            else
            {
                NOUS_ERROR_C(CURRENT_CHANNEL, "[ShaderHotReload] Compile failed for '%s': %s",
                             path.c_str(), result.errorMessage.c_str());
                if (result.shader)
                    NOUS_DELETE(result.shader, MemoryTag::RESOURCE_SHADER);
            }
        }

        m_inFlightJobCount.fetch_sub(1, std::memory_order_release);

    }, "Compiling: " + path);
}

bool RendererFrontend::ReloadShaderByPath(const std::string& path)
{
    if (!m_resourceManager)
        return false;

    const std::string normalizedIncoming = std::filesystem::path(path).generic_string();

    for (auto& [uid, resource] : m_resourceManager->GetResourcesMap())
    {
        if (resource->GetType() != ResourceType::SHADER)
            continue;

        const std::string normalizedStored =
            std::filesystem::path(resource->GetAssetsPath()).generic_string();

        if (normalizedIncoming != normalizedStored)
            continue;

        auto* shader = static_cast<ResourceShader*>(resource);
        DispatchCompileJob(normalizedIncoming, shader);
        return true;  // job dispatched (shader found)
    }

    NOUS_WARN_C(CURRENT_CHANNEL, "[ShaderHotReload] No loaded shader found for path '%s'.", path.c_str());
    return false;
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
