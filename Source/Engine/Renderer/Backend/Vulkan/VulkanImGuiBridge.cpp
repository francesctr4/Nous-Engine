#include <Engine/Renderer/Backend/Vulkan/VulkanImGuiBridge.h>
#include <Engine/Renderer/Backend/Vulkan/VulkanImGuiResources.h>
#include <Engine/Renderer/Backend/Vulkan/VulkanBackend.h>

void Engine_CreateGameViewportDescriptorSets()
{
    VulkanContext* ctx = VulkanBackend::GetVulkanContext();
    if (ctx)
        NOUS_ImGuiVulkanResources::CreateGameViewportDescriptorSets(ctx);
}

void Engine_CreateSceneViewportDescriptorSets()
{
    VulkanContext* ctx = VulkanBackend::GetVulkanContext();
    if (ctx)
        NOUS_ImGuiVulkanResources::CreateSceneViewportDescriptorSets(ctx);
}

uint64_t Engine_GetViewportTexture(void* descriptorSet)
{
    return NOUS_ImGuiVulkanResources::GetViewportTexture(
            reinterpret_cast<VkDescriptorSet>(descriptorSet));
}
