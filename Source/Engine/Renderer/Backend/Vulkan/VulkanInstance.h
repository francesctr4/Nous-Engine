#ifndef VULKANINSTANCE_H
#define VULKANINSTANCE_H

#include "VulkanTypes.inl"

// --------------- Vulkan Validation Layers --------------- //
const int8 c_VALIDATION_LAYERS_COUNT = 1;
const std::array<const char*, c_VALIDATION_LAYERS_COUNT> validationLayers = { "VK_LAYER_KHRONOS_validation" };

namespace NOUS_VulkanInstance
{
	bool CreateInstance(VulkanContext* vkContext);
	void DestroyInstance(VulkanContext* vkContext);

	bool CreateSurface(VulkanContext* vkContext);
	void DestroySurface(VulkanContext* vkContext);

	// -------------------------------------------------------------------------- //

	bool CheckValidationLayerSupport(const std::array<const char*, c_VALIDATION_LAYERS_COUNT>& validationLayers);

	void ShowSupportedExtensions();
	std::vector<const char*> GetRequiredExtensions();
}

#endif // VULKANINSTANCE_H