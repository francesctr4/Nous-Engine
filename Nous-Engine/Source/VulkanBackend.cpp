#include "VulkanBackend.h"
#include "VulkanTypes.inl"

#include "MemoryManager.h"
#include "Logger.h"

#include "DynamicArray.h"

VulkanContext* VulkanBackend::vkContext = nullptr;

VulkanBackend::VulkanBackend()
{
    vkContext = NOUS_NEW<VulkanContext>(MemoryManager::MemoryTag::RENDERER);
}

VulkanBackend::~VulkanBackend()
{
    NOUS_DELETE(vkContext, MemoryManager::MemoryTag::RENDERER);
}

bool VulkanBackend::Initialize()
{
    bool ret = true;

    NOUS_INFO("USING VULKAN");

    // TODO: Custom allocator
    vkContext->allocator = 0;

    if (!CreateInstance()) 
    {
        NOUS_ERROR("Failed to create Vulkan Instance. Shutting the Application.");
        ret = false;
    }

	return ret;
}

void VulkanBackend::Shutdown()
{
}

void VulkanBackend::Resized(uint16 width, uint16 height)
{
}

bool VulkanBackend::BeginFrame(float32 dt)
{
	return false;
}

bool VulkanBackend::EndFrame(float32 dt)
{
	return false;
}

// ------------- Vulkan Specific Functions ------------- \\

bool VulkanBackend::CreateInstance()
{
    bool ret = true;

    //if (enableValidationLayers && !CheckValidationLayerSupport(validationLayers)) {

    //    std::cout << "Validation Layers Requested, but not available!" << std::endl;

    //}

    //ShowSupportedExtensions();

    //std::vector<const char*> extensions = GetRequiredExtensions();

    VkApplicationInfo appInfo{};

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_3;
    appInfo.pNext = nullptr;

    appInfo.pApplicationName = TITLE;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);

    appInfo.pEngineName = TITLE;
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);

    VkInstanceCreateInfo createInfo{};

    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.pNext = nullptr;

    //createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    //createInfo.ppEnabledExtensionNames = extensions.data();

    //createInfo.enabledLayerCount = enableValidationLayers ? static_cast<uint32_t>(validationLayers.size()) : 0;
    //createInfo.ppEnabledLayerNames = enableValidationLayers ? validationLayers.data() : nullptr;

    createInfo.enabledExtensionCount = 0;
    createInfo.ppEnabledExtensionNames = 0;

    createInfo.enabledLayerCount = 0;
    createInfo.ppEnabledLayerNames = 0;

    //VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

    //if (enableValidationLayers) PopulateDebugMessengerCreateInfo(debugCreateInfo);

    //createInfo.pNext = enableValidationLayers ? (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo : nullptr;

    VkResult result = vkCreateInstance(&createInfo, vkContext->allocator, &vkContext->instance);

    if (result != VK_SUCCESS) 
    {
        NOUS_ERROR("vkCreateInstance failed with result: %u", result);
        ret = false;
    }
    else 
    {
        NOUS_INFO("Vulkan Instance created successfully!");
    }

    return ret;
}