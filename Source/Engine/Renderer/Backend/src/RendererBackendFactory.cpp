#include <RendererBackend/RendererBackendFactory.h>
#include <VulkanBackend/VulkanBackend.h>
#include <MemoryManager/MemoryManager.h>
#include <Logger/LogChannel.h>
#include <Logger/Logger.h>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_RENDERER_BACKEND;

IRendererBackend* CreateRendererBackend(const RendererBackendType type)
{
    IRendererBackend* backend = nullptr;

    switch (type)
    {
        case RendererBackendType::VULKAN:
            backend = NOUS_NEW<VulkanBackend>(MemoryTag::RENDERER);
            break;
        case RendererBackendType::OPENGL:
            // backend = NOUS_NEW<OpenGLBackend>(MemoryTag::RENDERER);
            break;
        case RendererBackendType::DIRECTX:
            // backend = NOUS_NEW<DirectXBackend>(MemoryTag::RENDERER);
            break;
        default:
            NOUS_ERROR_C(CURRENT_CHANNEL, "Unknown backend type: %d", static_cast<int>(type));
            return nullptr;
    }

    if (!backend)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create backend type (%d).", static_cast<int>(type));
        return nullptr;
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "Created backend type (%d) successfully.", static_cast<int>(type));
    return backend;
}
