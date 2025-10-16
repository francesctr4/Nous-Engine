#ifndef VULKANIMGUIRESOURCES_H
#define VULKANIMGUIRESOURCES_H

#include <Engine/Renderer/Backend/Vulkan/VulkanTypes.inl>
#include <Engine/Core/EngineExport.h>

typedef unsigned long long ImTextureID;

namespace NOUS_ImGuiVulkanResources
{
	NOUS_ENGINE_API void CreateImGuiVulkanResources(VulkanContext* vkContext);
	NOUS_ENGINE_API void DestroyImGuiVulkanResources(VulkanContext* vkContext);

	NOUS_ENGINE_API void RecreateImGuiVulkanResources(VulkanContext* vkContext);

	// ----------------------------------------------------------------------------------- //

	NOUS_ENGINE_API void CreateImGuiDescriptorPool(VulkanContext* vkContext);
	NOUS_ENGINE_API void CreateViewportTextureSampler(VulkanContext* vkContext, VkSampler* sampler);

	NOUS_ENGINE_API void CreateViewportImages(VulkanContext* vkContext);
	NOUS_ENGINE_API void CreateViewportDepthResources(VulkanContext* vkContext);

	// ----------------------------------------------------------------------------------- //

	NOUS_ENGINE_API void CreateSceneViewportDescriptorSets(VulkanContext* vkContext);
	NOUS_ENGINE_API void DestroySceneViewportDescriptorSets(VulkanContext* vkContext);

	NOUS_ENGINE_API void CreateGameViewportDescriptorSets(VulkanContext* vkContext);
	NOUS_ENGINE_API void DestroyGameViewportDescriptorSets(VulkanContext* vkContext);

	NOUS_ENGINE_API ImTextureID GetViewportTexture(VkDescriptorSet descriptorSet);

}

#endif // VULKANIMGUIRESOURCES_H