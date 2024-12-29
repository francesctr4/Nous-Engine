#pragma once

#include "VulkanTypes.inl"

namespace NOUS_ImGuiVulkanResources
{
	void CreateImGuiVulkanResources(VulkanContext* vkContext);
	void DestroyImGuiVulkanResources(VulkanContext* vkContext);

	// ----------------------------------------------------------------------------------- //

	void CreateImGuiDescriptorPool(VulkanContext* vkContext);

	void CreateImGuiImages(VulkanContext* vkContext, VulkanSwapChain* swapChain);

	void CreateImGuiRenderPass(VulkanContext* vkContext);
	void CreateImGuiPipeline(VulkanContext* vkContext);

	void CreateImGuiFramebuffers(VulkanContext* vkContext);

	void CreateImGuiCommandPool(VulkanContext* vkContext);
	void CreateImGuiCommandBuffers(VulkanContext* vkContext);

	void CreateImGuiTextureSampler(VulkanContext* vkContext);
	void CreateImGuiDescriptorSets(VulkanContext* vkContext);

	void RenderImGuiDrawData(VulkanContext* vkContext, VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderPass);
}