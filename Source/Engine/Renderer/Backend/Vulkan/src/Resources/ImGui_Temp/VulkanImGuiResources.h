#ifndef VULKANIMGUIRESOURCES_H
#define VULKANIMGUIRESOURCES_H

#include "VulkanTypes.inl"
namespace NOUS_ImGuiVulkanResources
{
	void CreateImGuiVulkanResources(VulkanContext* vkContext);
	void DestroyImGuiVulkanResources(VulkanContext* vkContext);

	void RecreateImGuiVulkanResources(VulkanContext* vkContext);

	// ----------------------------------------------------------------------------------- //

	void CreateImGuiDescriptorPool(VulkanContext* vkContext);
	void CreateViewportTextureSampler(VulkanContext* vkContext, VkSampler* sampler);

	void CreateViewportImages(VulkanContext* vkContext);
	void CreateViewportDepthResources(VulkanContext* vkContext);

	void CreatePickResources(VulkanContext* vkContext);
	void DestroyPickResources(VulkanContext* vkContext);

	// ----------------------------------------------------------------------------------- //

	unsigned long long GetViewportTexture(VkDescriptorSet descriptorSet);

}

#endif // VULKANIMGUIRESOURCES_H