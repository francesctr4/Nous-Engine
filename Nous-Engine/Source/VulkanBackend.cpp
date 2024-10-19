#include "VulkanBackend.h"
#include "VulkanTypes.inl"
#include "VulkanPlatform.h"

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

    ShowSupportedExtensions();

    DynamicArray<const char*> extensions = GetRequiredExtensions();

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

    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.GetLength());
    createInfo.ppEnabledExtensionNames = extensions.GetElements();

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

void VulkanBackend::ShowSupportedExtensions()
{
    uint32 extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    DynamicArray<VkExtensionProperties> supportedExtensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, supportedExtensions.GetElements());

    NOUS_DEBUG("Available Vulkan Extensions:\n");

    supportedExtensions.SetLength(extensionCount);

    for (int i = 0; i < supportedExtensions.GetLength(); ++i) 
    {
        NOUS_DEBUG("\t%s\n", supportedExtensions.GetElements()[i].extensionName);
    }
}

DynamicArray<const char*> VulkanBackend::GetRequiredExtensions()
{
    uint32 sdlExtensionCount = 0;

    if (!SDL_Vulkan_GetInstanceExtensions(GetSDLWindowData(), &sdlExtensionCount, nullptr)) 
    {
        NOUS_ERROR("Could not get the number of required instance extensions from SDL.");

    }

    DynamicArray<const char*> extensions(sdlExtensionCount);

    if (!SDL_Vulkan_GetInstanceExtensions(GetSDLWindowData(), &sdlExtensionCount, extensions.GetElements())) 
    {
        NOUS_ERROR("Could not get the name of required instance extensions from SDL.");

    }

    extensions.SetLength(sdlExtensionCount);

    extensions.Push(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

    if (enableValidationLayers) 
    {
        extensions.Push(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    NOUS_DEBUG("Required Vulkan Extensions:\n");

    for (int i = 0; i < extensions.GetLength(); ++i)
    {
        NOUS_DEBUG("\t%s\n", extensions.GetElements()[i]);
    }

    return extensions;
}
