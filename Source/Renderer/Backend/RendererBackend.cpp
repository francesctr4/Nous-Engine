#include "Renderer/Backend/RendererBackend.h"
#include "Renderer/Backend/Vulkan/VulkanBackend.h"
#include "Systems/Memory Manager/MemoryManager.h"

RendererBackend::RendererBackend() : backendInterface(nullptr), frameNumber(0)
{

}

RendererBackend::~RendererBackend()
{
    Destroy();
}

bool RendererBackend::Create(RendererBackendType bType)
{
	bool ret = false;

    switch (bType) 
    {
        case RendererBackendType::VULKAN:
        {
            backendInterface = NOUS_NEW<VulkanBackend>(MemoryManager::MemoryTag::RENDERER);
            ret = true;
            break;
        } 
        case RendererBackendType::OPENGL: 
        {
            //backendInterface = NOUS_NEW<OpenGLBackend>(MemoryManager::MemoryTag::RENDERER);
            ret = true;
            break;
        }
        case RendererBackendType::DIRECTX: 
        {
            //backendInterface = new DirectXBackend();
            ret = true;
            break;
        }
    }

	return ret;
}

void RendererBackend::Destroy()
{
    NOUS_DELETE(backendInterface, MemoryManager::MemoryTag::RENDERER);
}

bool RendererBackend::Initialize()
{
    if (backendInterface != nullptr) 
    {
        return backendInterface->Initialize();
    }

    return false;
}

void RendererBackend::Shutdown()
{
    if (backendInterface != nullptr)
    {
        backendInterface->Shutdown();
    }
}

void RendererBackend::Resized(uint16 width, uint16 height)
{
    if (backendInterface != nullptr)
    {
        backendInterface->Resized(width, height);
    }
}

FrameResult RendererBackend::BeginFrame(float dt)
{
    if (backendInterface)
        return backendInterface->BeginFrame(dt);

    return FrameResult::ERROR;
}

FrameResult RendererBackend::EndFrame(float dt)
{
    if (backendInterface)
        return backendInterface->EndFrame(dt);

    return FrameResult::ERROR;
}

bool RendererBackend::BeginRenderpass(RenderpassType renderpassID)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->BeginRenderpass(renderpassID);
    }

    return false;
}

bool RendererBackend::EndRenderpass(RenderpassType renderpassID)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->EndRenderpass(renderpassID);
    }

    return false;
}

bool RendererBackend::UpdateGlobalWorldState(RenderpassType renderpassID, glm::mat4x4 projection, glm::mat4x4 view, glm::vec3 viewPosition, glm::vec4 ambientColor, int32 mode)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->UpdateGlobalWorldState(renderpassID, projection, view, viewPosition, ambientColor, mode);
    }

    return false;
}

bool RendererBackend::UpdateGlobalUIState(RenderpassType renderpassID, glm::mat4x4 projection, glm::mat4x4 view, int32 mode)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->UpdateGlobalUIState(renderpassID, projection, view, mode);
    }

    return false;
}

bool RendererBackend::DrawGeometry(RenderpassType renderpassID, GeometryRenderData renderData)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->DrawGeometry(renderpassID, renderData);
    }

    return false;
}

bool RendererBackend::CreateTexture(const uint8* pixels, ResourceTexture* outTexture)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->CreateTexture(pixels, outTexture);
    }

    return false;
}

void RendererBackend::DestroyTexture(ResourceTexture* texture)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->DestroyTexture(texture);
    }
}

bool RendererBackend::CreateMaterial(ResourceMaterial* material)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->CreateMaterial(material);
    }

    return false;
}

void RendererBackend::DestroyMaterial(ResourceMaterial* material)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->DestroyMaterial(material);
    }
}

bool RendererBackend::CreateGeometry(uint32 vertexCount, const Vertex3D* vertices, uint32 indexCount, const uint32* indices, ResourceMesh* outGeometry)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->CreateGeometry(vertexCount, vertices, indexCount, indices, outGeometry);
    }

    return false;
}

void RendererBackend::DestroyGeometry(ResourceMesh* geometry)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->DestroyGeometry(geometry);
    }
}
