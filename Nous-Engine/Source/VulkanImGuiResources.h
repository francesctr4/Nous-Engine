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
	void CreateViewportTextureSampler(VulkanContext* vkContext, VkSampler* sampler);

	void CreateViewportImages(VulkanContext* vkContext);
	void CreateViewportDepthResources(VulkanContext* vkContext);

	// ----------------------------------------------------------------------------------- //

	void CreateSceneViewportDescriptorSets(VulkanContext* vkContext);
	void DestroySceneViewportDescriptorSets(VulkanContext* vkContext);

	void CreateGameViewportDescriptorSets(VulkanContext* vkContext);
	void DestroyGameViewportDescriptorSets(VulkanContext* vkContext);

	ImTextureID GetViewportTexture(VkDescriptorSet descriptorSet);

}