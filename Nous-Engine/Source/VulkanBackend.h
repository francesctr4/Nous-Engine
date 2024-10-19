#pragma once

#include "RendererBackend.h"

#ifdef _DEBUG
const bool enableValidationLayers = true;
#else
const bool enableValidationLayers = false;
#endif

template<typename T>
class DynamicArray;

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

	// ------------------------------------ Vulkan Helper Functions ------------------------------------ \\
	
	// --------------- Validation Layers ---------------

	//bool CheckValidationLayerSupport(const std::vector<const char*>& validationLayers);

	// --------------- Vulkan Extensions ---------------

	void ShowSupportedExtensions();
	DynamicArray<const char*> GetRequiredExtensions();

	//// --------------- Validation Layers Debug Messenger ---------------

	//static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
	//	VkDebugUtilsMessageTypeFlagsEXT messageType,
	//	const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
	//	void* pUserData);

	//void PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

private:

	static VulkanContext* vkContext;

};