#pragma once

#include "VulkanTypes.inl"

typedef unsigned long long ImTextureID;

namespace NOUS_ImGuiVulkanResources
{
	void CreateImGuiVulkanResources(VulkanContext* vkContext);
	void DestroyImGuiVulkanResources(VulkanContext* vkContext);

	void RecreateImGuiVulkanResources(VulkanContext* vkContext);

	// ----------------------------------------------------------------------------------- //

	void CreateImGuiDescriptorPool(VulkanContext* vkContext);
	void CreateImGuiTextureSampler(VulkanContext* vkContext);

	void CreateViewportImages(VulkanContext* vkContext);
	void CreateDepthResources(VulkanContext* vkContext);

	// ----------------------------------------------------------------------------------- //

	void CreateViewportDescriptorSets(VulkanContext* vkContext);
	ImTextureID GetViewportTexture(VulkanContext* vkContext, uint32 imageIndex);
	void DestroyViewportDescriptorSets(VulkanContext* vkContext);
}