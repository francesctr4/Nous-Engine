#include "RendererBackend.h"

#include "VulkanBackend.h"

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
            //backendInterface = new OpenGLBackend();
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

bool RendererBackend::BeginFrame(float32 dt)
{
    if (backendInterface != nullptr) 
    {
        return backendInterface->BeginFrame(dt);
    }

    return false;
}

bool RendererBackend::EndFrame(float32 dt)
{
    if (backendInterface != nullptr) 
    {
        return backendInterface->EndFrame(dt);
    }

    return false;
}
