#include "Engine/Renderer/Backend/RendererBackend.h"
#include "Engine/Renderer/Backend/Vulkan/VulkanBackend.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Core/Logger/LogChannel.h"
#include "Engine/Core/Logger/Logger.h"

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_RENDERER_BACKEND;

RendererBackend::RendererBackend() : mBackendInterface(nullptr), mFrameNumber(0), mBackendType(RendererBackendType::UNKNOWN)
{

}

RendererBackend::~RendererBackend()
{
    Destroy();
}

// ─────────────────────────────── Lifecycle ───────────────────────────────
bool RendererBackend::Create(RendererBackendType bType)
{
    mBackendType = bType;

    switch (bType)
    {
        case RendererBackendType::VULKAN:
            mBackendInterface = NOUS_NEW<VulkanBackend>(MemoryTag::RENDERER);
            break;
        case RendererBackendType::OPENGL:
            // backendInterface = NOUS_NEW<OpenGLBackend>(MemoryTag::RENDERER);
            break;
        case RendererBackendType::DIRECTX:
            // backendInterface = NOUS_NEW<DirectXBackend>(MemoryTag::RENDERER);
            break;
        default:
            NOUS_ERROR_C(CURRENT_CHANNEL, "Unknown backend type: %d", static_cast<int>(bType));
            return false;
    }

    if (!mBackendInterface)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create backend interface.");
        return false;
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "Created backend type (%d) successfully.", static_cast<int>(bType));
    return true;
}

void RendererBackend::Destroy()
{
    if (mBackendInterface)
    {
        NOUS_INFO_C(CURRENT_CHANNEL, "Destroying renderer backend...");
        NOUS_DELETE(mBackendInterface, MemoryTag::RENDERER);
    }
}

void RendererBackend::InjectDependencies(
    EventSystem* eventSystem,
    NOUS_Multithreading::NOUS_JobSystem* jobSystem,
    ModuleWindow* window,
    ModuleResourceManager* resourceManager)
{
    if (mBackendInterface)
        mBackendInterface->InjectDependencies(eventSystem, jobSystem, window, resourceManager);
}

bool RendererBackend::Initialize()
{
    if (!mBackendInterface)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Initialize called with no backend.");
        return false;
    }
    return mBackendInterface->Initialize();
}

void RendererBackend::Shutdown()
{
    if (mBackendInterface)
    {
        mBackendInterface->Shutdown();
    }
}

void RendererBackend::SetRenderMode(RenderMode mode) noexcept
{
    if (mBackendInterface)
        mBackendInterface->SetRenderMode(mode);
}

void RendererBackend::ReleaseFrameResources() noexcept
{
    if (mBackendInterface)
    {
        mBackendInterface->ReleaseFrameResources();
    }
}

void RendererBackend::Resized(uint16_t width, uint16_t height)
{
    if (mBackendInterface)
    {
        mBackendInterface->Resized(width, height);
    }
}

// ─────────────────────────────── Frame Lifecycle ─────────────────────────
FrameResult RendererBackend::BeginFrame(float dt)
{
    return mBackendInterface ? mBackendInterface->BeginFrame(dt) : FrameResult::ERROR;
}

FrameResult RendererBackend::EndFrame(float dt)
{
    return mBackendInterface ? mBackendInterface->EndFrame(dt) : FrameResult::ERROR;
}

// ─────────────────────────────── Renderpasses ────────────────────────────
bool RendererBackend::BeginRenderpass(RenderpassType renderpassID)
{
    return mBackendInterface && mBackendInterface->BeginRenderpass(renderpassID);
}

bool RendererBackend::EndRenderpass(RenderpassType renderpassID)
{
    return mBackendInterface && mBackendInterface->EndRenderpass(renderpassID);
}

// ─────────────────────────────── Global State ────────────────────────────
bool RendererBackend::UpdateGlobalWorldState(RenderpassType renderpassID,
                                             const GlobalUBO& globalUBO)
{
    return mBackendInterface && mBackendInterface->UpdateGlobalWorldState(renderpassID, globalUBO);
}

// ─────────────────────────────── Drawing ─────────────────────────────────
bool RendererBackend::DrawGeometry(RenderpassType renderpassID, const GeometryRenderData& renderData)
{
    return mBackendInterface && mBackendInterface->DrawGeometry(renderpassID, renderData);
}

void RendererBackend::UploadInstanceMatrices(uint32_t frameIndex,
                                              const glm::mat4* matrices,
                                              uint32_t count,
                                              uint32_t instanceOffset)
{
    if (mBackendInterface)
        mBackendInterface->UploadInstanceMatrices(frameIndex, matrices, count, instanceOffset);
}

bool RendererBackend::DrawGeometryBatched(RenderpassType renderpassID,
                                           const InstancedBatch& batch)
{
    return mBackendInterface && mBackendInterface->DrawGeometryBatched(renderpassID, batch);
}

// ─────────────────────────────── Resources ───────────────────────────────
bool RendererBackend::CreateTexture(const uint8* pixels, ResourceTexture* outTexture)
{
    return mBackendInterface && mBackendInterface->CreateTexture(pixels, outTexture);
}

void RendererBackend::DestroyTexture(ResourceTexture* texture)
{
    if (mBackendInterface)
    {
        mBackendInterface->DestroyTexture(texture);
    }
}

bool RendererBackend::CreateMaterial(ResourceMaterial* material)
{
    return mBackendInterface && mBackendInterface->CreateMaterial(material);
}

void RendererBackend::DestroyMaterial(ResourceMaterial* material)
{
    if (mBackendInterface)
    {
        mBackendInterface->DestroyMaterial(material);
    }
}

bool RendererBackend::CreateGeometry(uint32 vertexCount, const Vertex3D* vertices,
                                     uint32 indexCount, const uint32* indices,
                                     ResourceMesh* outGeometry)
{
    return mBackendInterface && mBackendInterface->CreateGeometry(vertexCount, vertices, indexCount, indices, outGeometry);
}

bool RendererBackend::CreateShader(ResourceShader* shader)
{
    return mBackendInterface && mBackendInterface->CreateShader(shader);
}

void RendererBackend::DestroyShader(ResourceShader* shader)
{
    if (mBackendInterface)
        mBackendInterface->DestroyShader(shader);
}

bool RendererBackend::ReloadShader(ResourceShader* shader) noexcept
{
    return mBackendInterface && mBackendInterface->ReloadShader(shader);
}

bool RendererBackend::ApplyCompiledShader(ResourceShader* shader) noexcept
{
    return mBackendInterface && mBackendInterface->ApplyCompiledShader(shader);
}

void RendererBackend::DestroyGeometry(ResourceMesh* geometry)
{
    if (mBackendInterface)
    {
        mBackendInterface->DestroyGeometry(geometry);
    }
}

uint32_t RendererBackend::PickObjectAt(int32_t pixelX, int32_t pixelY,
                                       const glm::mat4& projection, const glm::mat4& view,
                                       const std::vector<GeometryRenderData>& geometries)
{
    return mBackendInterface ? mBackendInterface->PickObjectAt(pixelX, pixelY, projection, view, geometries) : 0;
}

bool RendererBackend::DrawOutlinedGeometries(RenderpassType renderpassID,
                                             const glm::mat4& projection, const glm::mat4& view,
                                             const std::vector<GeometryRenderData>& outlinedGeometries,
                                             const OutlineSettings& settings)
{
    return mBackendInterface && mBackendInterface->DrawOutlinedGeometries(renderpassID, projection, view, outlinedGeometries, settings);
}

bool RendererBackend::DrawGrid(RenderpassType renderpassID,
                               const glm::mat4& projection, const glm::mat4& view)
{
    return mBackendInterface && mBackendInterface->DrawGrid(renderpassID, projection, view);
}

bool RendererBackend::DrawBackground(RenderpassType renderpassID,
                               const glm::mat4& projection, const glm::mat4& view)
{
    return mBackendInterface && mBackendInterface->DrawBackground(renderpassID, projection, view);
}

bool RendererBackend::DrawBoundingBoxes(RenderpassType renderpassID,
                                        const glm::mat4& projection, const glm::mat4& view,
                                        const std::vector<BoundingBoxData>& boxes)
{
    return mBackendInterface && mBackendInterface->DrawBoundingBoxes(renderpassID, projection, view, boxes);
}

bool RendererBackend::DrawCameraFrustums(RenderpassType renderpassID, const glm::mat4 &projection,
    const glm::mat4 &view, const std::vector<CameraFrustumData> &frustums, bool globalAlreadySet)
{
    return mBackendInterface && mBackendInterface->DrawCameraFrustums(renderpassID, projection, view, frustums, globalAlreadySet);
}

bool RendererBackend::DrawPointLightDebugs(RenderpassType renderpassID, const glm::mat4& projection,
    const glm::mat4& view, const std::vector<BoundingBoxData>& lightDebugs, bool globalAlreadySet)
{
    return mBackendInterface && mBackendInterface->DrawPointLightDebugs(renderpassID, projection, view, lightDebugs, globalAlreadySet);
}
