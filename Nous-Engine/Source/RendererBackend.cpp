#include "RendererBackend.h"

#include "VulkanBackend.h"
#include "OpenGLBackend.h"

#include "MemoryManager.h"

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

bool RendererBackend::Initalize()
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

bool RendererBackend::BeginFrame(float dt)
{
    if (backendInterface != nullptr) 
    {
        return backendInterface->BeginFrame(dt);
    }

    return false;
}

bool RendererBackend::EndFrame(float dt)
{
    if (backendInterface != nullptr) 
    {
        return backendInterface->EndFrame(dt);
    }

    return false;
}

bool RendererBackend::BeginRenderpass(BuiltInRenderpass renderpassID)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->BeginRenderpass(renderpassID);
    }

    return false;
}

bool RendererBackend::EndRenderpass(BuiltInRenderpass renderpassID)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->EndRenderpass(renderpassID);
    }

    return false;
}

void RendererBackend::UpdateGlobalWorldState(BuiltInRenderpass renderpassID, float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->UpdateGlobalWorldState(renderpassID, projection, view, viewPosition, ambientColor, mode);
    }
}

void RendererBackend::UpdateGlobalUIState(BuiltInRenderpass renderpassID, float4x4 projection, float4x4 view, int32 mode)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->UpdateGlobalUIState(renderpassID, projection, view, mode);
    }
}

void RendererBackend::DrawGeometry(BuiltInRenderpass renderpassID, GeometryRenderData renderData)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->DrawGeometry(renderpassID, renderData);
    }
}

void RendererBackend::CreateTexture(const uint8* pixels, ResourceTexture* outTexture)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->CreateTexture(pixels, outTexture);
    }
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
}

void RendererBackend::DestroyGeometry(ResourceMesh* geometry)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->DestroyGeometry(geometry);
    }
}
