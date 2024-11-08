#pragma once

#include "RendererBackend.h"

#include "VulkanTypes.inl"

// --------------- Vulkan Validation Layers --------------- \\

const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" };

#ifdef _DEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif

// --------------- Debug Messenger ---------------

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const
	VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const
	VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT*
	pDebugMessenger);

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
	VkDebugUtilsMessengerEXT debugMessenger, const
	VkAllocationCallbacks* pAllocator);

// --------------- Vulkan Renderer Backend --------------- \\

struct VulkanContext;

class VulkanBackend : public IRendererBackend 
{
public:

	VulkanBackend();
	virtual ~VulkanBackend() override;

	bool Initialize() override;
	void Shutdown() override;

	void Resized(uint16 width, uint16 height) override;

	bool BeginFrame(float32 dt) override;
	bool EndFrame(float32 dt) override;

	// ------------------------------------ Vulkan Pipeline Functions ------------------------------------ \\

	bool CreateInstance();
	bool SetupDebugMessenger();
	bool CreateSurface();

	// ------------------------------------ Vulkan Helper Functions ------------------------------------ \\
	
	// --------------- Validation Layers --------------- \\

	bool CheckValidationLayerSupport(const std::vector<const char*>& validationLayers);

	// --------------- Extensions --------------- \\

	void ShowSupportedExtensions();
	DynamicArray<const char*> GetRequiredExtensions();

	// --------------- Debug Messenger --------------- \\

	static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
		VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
		void* pUserData);

	void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& debugCreateInfo);

private:

	static VulkanContext* vkContext;

};