#ifndef NOUS_ENGINE_VULKANIMGUIBRIDGE_H
#define NOUS_ENGINE_VULKANIMGUIBRIDGE_H

#include <Engine/Core/EngineExport.h>     // for NOUS_ENGINE_API
#include <Engine/Renderer/Backend/Vulkan/VulkanTypes.inl>

extern "C" {

// Expose a simple C-style entry point callable from any DLL
NOUS_ENGINE_API void Engine_CreateGameViewportDescriptorSets();
NOUS_ENGINE_API void Engine_CreateSceneViewportDescriptorSets();
NOUS_ENGINE_API uint64_t Engine_GetViewportTexture(void* descriptorSet);

}

#endif //NOUS_ENGINE_VULKANIMGUIBRIDGE_H
