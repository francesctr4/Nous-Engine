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
            backendInterface = NOUS_NEW<OpenGLBackend>(MemoryManager::MemoryTag::RENDERER);
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

    backendInterface->defaultDiffuse = defaultDiffuse;

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

void RendererBackend::UpdateGlobalState(float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->UpdateGlobalState(projection, view, viewPosition, ambientColor, mode);
    }
}

void RendererBackend::UpdateObject(GeometryRenderData renderData)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->UpdateObject(renderData);
    }
}

void RendererBackend::CreateTexture(const char* path, bool autoRelease, int32 width, int32 height, int32 channelCount, const uint8* pixels, bool hasTransparency, Texture* outTexture)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->CreateTexture(path, autoRelease, width, height, channelCount, pixels, hasTransparency, outTexture);
    }
}

void RendererBackend::DestroyTexture(Texture* texture)
{
    if (backendInterface != nullptr)
    {
        return backendInterface->DestroyTexture(texture);
    }
}