#ifndef VULKANRENDERPASS_H
#define VULKANRENDERPASS_H

#include "Engine/Renderer/Backend/Vulkan/VulkanTypes.inl"

enum RenderpassClearFlag: uint8_t
{
    NO_CLEAR,

    COLOR_BUFFER,
    DEPTH_BUFFER,
    STENCIL_BUFFER
};

namespace NOUS_VulkanRenderpass
{
    /**
     * @brief Create a renderpass.
     *
     * @param offscreen  When true the color attachment transitions to
     *                   VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL (suitable for sampling
     *                   in a later pass, e.g. scene/game viewports).
     *                   When false it transitions to VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
     *                   (suitable for swapchain presentation, e.g. the UI pass).
     */
    bool CreateRenderpass(
        VulkanContext* vkContext,
        VulkanRenderpass* outRenderpass,
        glm::vec4 renderArea, glm::vec4 clearColor,
        float depth, uint32 stencil, uint8 clearFlags,
        bool prevPass, bool nextPass,
        bool offscreen = false,
        VkFormat colorFormatOverride = VK_FORMAT_UNDEFINED);

    void DestroyRenderpass(VulkanContext* vkContext, VulkanRenderpass* renderpass);

    void BeginRenderpass(VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderpass, VkFramebuffer frameBuffer);

    void EndRenderpass(VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderpass);
}

#endif // VULKANRENDERPASS_H
