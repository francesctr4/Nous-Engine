#ifndef VULKANDEBUGMESSENGER_H
#define VULKANDEBUGMESSENGER_H

#include "Engine/Renderer/Backend/Vulkan/VulkanTypes.inl"

namespace NOUS_VulkanDebugMessenger
{
	VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const
		VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const
		VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT*
		pDebugMessenger);

	void DestroyDebugUtilsMessengerEXT(VkInstance instance,
		VkDebugUtilsMessengerEXT debugMessenger, const
		VkAllocationCallbacks* pAllocator);

	// ----------------------------------------------------------------------------------------------------- //

	bool SetupDebugMessenger(VulkanContext* vkContext);

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& debugCreateInfo);

}

#endif // VULKANDEBUGMESSENGER_H