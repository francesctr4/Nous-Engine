#pragma once

#include "VulkanTypes.inl"

namespace NOUS_ImGuiVulkanResources
{
	void CreateImGuiVulkanResources(VulkanContext* vkContext);
	void DestroyImGuiVulkanResources(VulkanContext* vkContext);
	void RecreateImGuiVulkanResources(VulkanContext* vkContext);

	// ----------------------------------------------------------------------------------- //

	void CreateImGuiDescriptorPool(VulkanContext* vkContext);

	/*void CreateImGuiImages(VulkanContext* vkContext, VulkanSwapChain* swapChain);*/

	/*void CreateImGuiRenderPass(VulkanContext* vkContext);
	void CreateImGuiPipeline(VulkanContext* vkContext);*/

	void CreateImGuiFramebuffers(VulkanContext* vkContext);

	//void CreateImGuiCommandPool(VulkanContext* vkContext);
	//void CreateImGuiCommandBuffers(VulkanContext* vkContext);

	void CreateImGuiTextureSampler(VulkanContext* vkContext);
	/*void CreateImGuiDescriptorSets(VulkanContext* vkContext);*/

	/*void RenderImGuiDrawData(VulkanContext* vkContext, VulkanCommandBuffer* commandBuffer, VulkanRenderpass* renderPass);*/

	// -------------------------------------------------------- Second try --------------------------------- //

	void InsertImageMemoryBarrier(
		VkCommandBuffer cmdbuffer,
		VkImage image,
		VkAccessFlags srcAccessMask,
		VkAccessFlags dstAccessMask,
		VkImageLayout oldImageLayout,
		VkImageLayout newImageLayout,
		VkPipelineStageFlags srcStageMask,
		VkPipelineStageFlags dstStageMask,
		VkImageSubresourceRange subresourceRange);

	VkCommandBuffer BeginSingleTimeCommands(VulkanContext* vkContext, const VkCommandPool& cmdPool);

	void EndSingleTimeCommands(VulkanContext* vkContext, VkCommandBuffer commandBuffer, const VkCommandPool& cmdPool);

	void CreateViewportImages(VulkanContext* vkContext);

	VkImageView CreateImageView(VulkanContext* vkContext, VkImage image, VkFormat format, VkImageAspectFlags aspectFlags);

	void CreateViewportImageViews(VulkanContext* vkContext);

	void CreateDepthResources(VulkanContext* vkContext);

	void DestroyDepthResources(VulkanContext* vkContext);
}