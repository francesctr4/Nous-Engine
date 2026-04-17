#include "Engine/Renderer/Frontend/RendererFrontend.h"
#include "Engine/Renderer/Backend/RendererBackend.h"

#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceShader/include/ResourceShader.h"
#include "Engine/Systems/ResourceManager/Resource/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ShaderSystem/ShaderLoader/include/ShaderLoader.h"
#include "Engine/Systems/ShaderSystem/ShaderCompiler/include/ShaderCompilerTypes.h"
#include "Engine/NOUS_Multithreading/NOUS_JobSystem/include/NOUS_JobSystem.h"
#include "Engine/Core/Logger/Asserts.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

#ifdef _PROFILING
#include <tracy/Tracy.hpp>
#endif

#include "Engine/Systems/CameraSystem/Camera/include/Camera.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Renderer/Frontend/IEditorOverlay.h"

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_RENDERER_FRONTEND;

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

bool RendererFrontend::Initialize(const RendererBackendType backendType)
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

void RendererFrontend::Shutdown() const
{
	if (!mBackend) return;

	// Wait for any in-flight compile jobs to finish before tearing down GPU resources.
	// Jobs hold a ResourceShader* and push to m_readySwaps — both must remain valid.
	while (m_inFlightJobCount.load(std::memory_order_acquire) > 0) { /* spin */ }

	mBackend->Shutdown();
	mBackend->Destroy();
}

void RendererFrontend::ReleaseFrameResources() const noexcept
{
    if (mBackend)
        mBackend->ReleaseFrameResources();
}

void RendererFrontend::OnResized(const uint16_t width, const uint16_t height) const
{
	mBackend->Resized(width, height);
}

FrameResult RendererFrontend::BeginFrame(const float dt) const
{
	return mBackend->BeginFrame(dt);
}

FrameResult RendererFrontend::EndFrame(const float dt) const
{
	const FrameResult result = mBackend->EndFrame(dt);

	if (result == FrameResult::SUCCESS)
	{
		mBackend->mFrameNumber++;
	}

	return result;
}

RendererFrontend::GroupedGeometries RendererFrontend::GroupGeometries(
    const std::vector<GeometryRenderData>& geometries,
    uint32_t baseInstance) const
{
    GroupedGeometries result;
    if (geometries.empty()) return result;

    // Sort so same (mesh, material) pairs are contiguous — required for correct firstInstance offsets.
    std::vector<const GeometryRenderData*> sorted;
    sorted.reserve(geometries.size());
    for (const auto& g : geometries) sorted.push_back(&g);
    std::sort(sorted.begin(), sorted.end(), [](const GeometryRenderData* a, const GeometryRenderData* b) {
        if (a->material != b->material) return a->material < b->material;
        return a->geometry < b->geometry;
    });

    // Data is sorted, so identical (geometry, material) pairs are always adjacent.
    // Just compare with the last batch instead of a hash map — no collision risk.
    for (const GeometryRenderData* grd : sorted)
    {
        if (!grd->geometry || !grd->material) continue;
        if (result.matrices.size() >= c_maxInstances) break;

        // Local index within this pass's matrix array; add baseInstance to get SSBO index.
        const uint32_t localIndex = static_cast<uint32_t>(result.matrices.size());
        const uint32_t ssboIndex  = baseInstance + localIndex;
        result.matrices.push_back(grd->model);

        if (!result.batches.empty() &&
            result.batches.back().geometry == grd->geometry &&
            result.batches.back().material == grd->material)
        {
            result.batches.back().instanceCount++;
        }
        else
        {
            InstancedBatch batch;
            batch.geometry      = grd->geometry;
            batch.material      = grd->material;
            batch.firstInstance = ssboIndex;
            batch.instanceCount = 1;
            result.batches.push_back(batch);
        }
    }

    return result;
}

FrameResult RendererFrontend::DrawFrame(RenderPacket* packet) const
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
	{
#ifdef _PROFILING
		ZoneScopedN("BeginFrame");
#endif
		switch (BeginFrame(packet->deltaTime))
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
	}

	bool success = true;

	// Assembles the full GlobalUBO from camera + light data in the render packet.
	auto MakeGlobalUBO = [&](const Camera* camera) -> GlobalUBO
	{
		GlobalUBO ubo{};
		ubo.projection    = camera->GetProjectionMatrix();
		ubo.view          = camera->GetViewMatrix();
		ubo.viewPosition  = glm::vec4(camera->GetPos(), 0.f);
		ubo.ambientColor  = glm::vec4(1.f, 1.f, 1.f, 0.1f);
		ubo.directionalLight = packet->directionalLight;
		ubo.lightCountAndPad.x = static_cast<int32_t>(packet->activePointLightCount);
		std::copy_n(packet->pointLights,
		          packet->activePointLightCount,
		          ubo.pointLights);
		ubo.time = glm::vec4(packet->totalTime,
		                     std::sin(packet->totalTime),
		                     std::cos(packet->totalTime),
		                     packet->deltaTime);
		return ubo;
	};

	// --- SCENE PASS (EDITOR mode only) ---
	if (mRenderMode == RenderMode::EDITOR)
	{
#ifdef _PROFILING
		ZoneScopedN("Scene Pass");
#endif
		constexpr auto sceneRenderpass = RenderpassType::SCENE;
		success &= ExecuteRenderpass(sceneRenderpass, [&]
		{
			success &= mBackend->DrawBackground(sceneRenderpass,
				packet->editorCamera->GetProjectionMatrix(),
				packet->editorCamera->GetViewMatrix());

			// IMPORTANT: DrawGrid (and DrawBackground above) each call UpdateGlobal on
			// their own shader, which rebinds set=0 to their own tiny UBO buffer. Because
			// the set=0 layouts are structurally compatible (one UBO at binding 0) across
			// all shaders in this pass, Vulkan PRESERVES the set=0 binding when pipelines
			// switch. If UpdateGlobalWorldState runs before DrawGrid, the grid call will
			// clobber the MaterialShader's set=0 binding, and subsequent DrawGeometry
			// calls will read light data (offsets > 128 bytes) past the grid UBO's range
			// — they see zeros, lights go dark. Run UpdateGlobalWorldState AFTER all
			// scenery draws that manage their own set=0, so the last binding on the main
			// command buffer is MaterialShader's full 720-byte GlobalUBO.
			success &= mBackend->DrawGrid(
					sceneRenderpass,
					packet->editorCamera->GetProjectionMatrix(),
					packet->editorCamera->GetViewMatrix());

			success &= mBackend->UpdateGlobalWorldState(sceneRenderpass,
				MakeGlobalUBO(packet->editorCamera));

			{
#ifdef _PROFILING
				ZoneScopedN("DrawGeometryBatched (Scene)");
#endif
				// Scene pass uses SSBO range [0, c_maxInstances).
				const GroupedGeometries grouped = GroupGeometries(packet->geometries, 0);
				if (!grouped.matrices.empty())
				{
					mBackend->UploadInstanceMatrices(
						static_cast<uint32_t>(mBackend->mFrameNumber % 3),
						grouped.matrices.data(),
						static_cast<uint32_t>(grouped.matrices.size()),
						0);
				}
				for (const auto& batch : grouped.batches)
					success &= mBackend->DrawGeometryBatched(sceneRenderpass, batch);
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

			if (!mPointLightDebugs.empty())
			{
				success &= mBackend->DrawPointLightDebugs(
					sceneRenderpass,
					packet->editorCamera->GetProjectionMatrix(),
					packet->editorCamera->GetViewMatrix(),
					mPointLightDebugs,
					/*globalAlreadySet=*/ !mBoundingBoxes.empty() || !mCameraFrustums.empty());
			}
		});
	}

	// --- GAME PASS ---
	{
#ifdef _PROFILING
		ZoneScopedN("Game Pass");
#endif
		constexpr auto gameRenderpass = RenderpassType::GAME;
		success &= ExecuteRenderpass(gameRenderpass, [&]
		{
			success &= mBackend->DrawBackground(gameRenderpass,
				packet->gameCamera->GetProjectionMatrix(),
				packet->gameCamera->GetViewMatrix());

			success &= mBackend->UpdateGlobalWorldState(gameRenderpass,
				MakeGlobalUBO(packet->gameCamera));

			{
#ifdef _PROFILING
				ZoneScopedN("DrawGeometryBatched (Game)");
#endif
				// In EDITOR mode, gameGeometries is game-camera-culled.
				// In GAME mode, the sole pass uses geometries (same list).
				const auto& gameList = (mRenderMode == RenderMode::EDITOR)
				    ? packet->gameGeometries
				    : packet->geometries;
				// Game pass uses SSBO range [c_maxInstances, 2*c_maxInstances) to avoid
				// overwriting scene matrices that the GPU hasn't consumed yet.
				const GroupedGeometries groupedGame = GroupGeometries(gameList, c_maxInstances);
				if (!groupedGame.matrices.empty())
				{
					mBackend->UploadInstanceMatrices(
						static_cast<uint32_t>(mBackend->mFrameNumber % 3),
						groupedGame.matrices.data(),
						static_cast<uint32_t>(groupedGame.matrices.size()),
						c_maxInstances);
				}
				for (const auto& batch : groupedGame.batches)
					success &= mBackend->DrawGeometryBatched(gameRenderpass, batch);
			}
		});
	}

	// --- UI PASS (EDITOR mode only) ---
	if (mRenderMode == RenderMode::EDITOR)
	{
#ifdef _PROFILING
		ZoneScopedN("UI Pass");
#endif
		constexpr auto uiRenderpass = RenderpassType::UI;
		success &= ExecuteRenderpass(uiRenderpass, [&]
		{
#ifdef _PROFILING
			ZoneScopedN("DrawEditor (ImGui)");
#endif
			DrawEditor();
		});
	}

	// --- END FRAME ---
	FrameResult endResult;
	{
#ifdef _PROFILING
		ZoneScopedN("EndFrame");
#endif
		endResult = EndFrame(packet->deltaTime);
	}

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

bool RendererFrontend::ExecuteRenderpass(RenderpassType pass, const std::function<void()>& drawCommands) const
{
	if (!mBackend->BeginRenderpass(pass))
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "Begin Renderpass with type (%d) failed!", static_cast<int>(pass));
		return false;
	}

	drawCommands();

	if (!mBackend->EndRenderpass(pass))
	{
		NOUS_ERROR_C(CURRENT_CHANNEL, "End Renderpass with type (%d) failed!", static_cast<int>(pass));
		return false;
	}

	return true;
}

void RendererFrontend::DrawEditor() const
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

bool RendererFrontend::CreateGeometry(const uint32_t vertexCount, const Vertex3D* vertices, const uint32_t indexCount, const uint32_t* indices, ResourceMesh* outGeometry)
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

        auto* shader = down_cast<ResourceShader*>(resource);
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
        std::lock_guard lock(m_swapQueueMutex);
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

void RendererFrontend::RequestMaterialShaderChange(ResourceMaterial* material,
                                                    ResourceShader* newShader)
{
    std::lock_guard lock(m_reslotMutex);
    m_pendingReslots.push_back({material, newShader});
}

void RendererFrontend::FlushPendingReslots()
{
    std::vector<PendingReslot> pending;
    {
        std::lock_guard lock(m_reslotMutex);
        pending.swap(m_pendingReslots);
    }

    for (auto& [mat, newShader] : pending)
    {
        mBackend->DestroyMaterial(mat);
        mat->SetShader(newShader);
        NOUS_ASSERT(mBackend->CreateMaterial(mat));
    }
}

void RendererFrontend::DispatchCompileJob(const std::string& path, ResourceShader* shader)
{
    {
        std::lock_guard lock(m_swapQueueMutex);
        if (m_inFlightPaths.contains(path))
            return;  // compile already in-flight for this path — skip duplicate
        m_inFlightPaths.insert(path);
    }

    m_inFlightJobCount.fetch_add(1, std::memory_order_relaxed);

    m_jobSystem->SubmitJob([this, path, shader]
    {
        NOUS_INFO_C(CURRENT_CHANNEL, "[RendererFrontend::DispatchCompileJob] Compiling '%s'...", path.c_str());

        const ShaderCompilerConfig config;
        ShaderLoadResult result = NOUS_ShaderSystem::LoadShaderFromFile(path, config);

        {
            std::lock_guard lock(m_swapQueueMutex);
            m_inFlightPaths.erase(path);

            if (result.success)
            {
                m_readySwaps.push_back({ shader, std::move(result) });
            }
            else
            {
                NOUS_ERROR_C(CURRENT_CHANNEL, "[RendererFrontend::DispatchCompileJob] Compile failed for '%s': %s",
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

        auto* shader = down_cast<ResourceShader*>(resource);
        DispatchCompileJob(normalizedIncoming, shader);
        return true;  // job dispatched (shader found)
    }

    NOUS_WARN_C(CURRENT_CHANNEL, "[ShaderHotReload] No loaded shader found for path '%s'.", path.c_str());
    return false;
}

uint32_t RendererFrontend::PickObjectAt(const int32_t pixelX, const int32_t pixelY,
                                        const glm::mat4& projection, const glm::mat4& view,
                                        const std::vector<GeometryRenderData>& geometries) const
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

void RendererFrontend::SetPointLightDebugs(const std::vector<BoundingBoxData>& lightDebugs)
{
	mPointLightDebugs = lightDebugs;
}

void RendererFrontend::SetEditorOverlay(IEditorOverlay *overlay)
{
    mEditorOverlay = overlay;
}

void RendererFrontend::SetRenderMode(const RenderMode mode) noexcept
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

void RendererFrontend::SetBackendType(const RendererBackendType backendType) noexcept
{
    mBackendType = backendType;
}
