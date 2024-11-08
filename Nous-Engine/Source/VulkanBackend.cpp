#include "VulkanBackend.h"
#include "VulkanPlatform.h"
#include "VulkanDevice.h"
#include "VulkanSwapchain.h"
#include "VulkanRenderpass.h"
#include "VulkanCommandBuffer.h"

#include "MemoryManager.h"
#include "Logger.h"

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

    NOUS_INFO(" ----------------------- USING VULKAN BACKEND ----------------------- ");

    // TODO: Custom allocator
    vkContext->allocator = 0;

    // Get Framebuffer Size
    GetFramebufferSize(&vkContext->framebufferWidth, &vkContext->framebufferHeight);

    // Instance
    NOUS_DEBUG("Creating Vulkan instance...");
    if (!CreateInstance()) 
    {
        NOUS_ERROR("Failed to create Vulkan Instance. Shutting the Application.");
        ret = false;
    }
    else 
    {
        NOUS_DEBUG("Vulkan Instance created successfully!");
    }

    // Debugger
    NOUS_DEBUG("Creating Vulkan Debugger...");
    if (!SetupDebugMessenger()) 
    {
        NOUS_ERROR("Failed to create Vulkan Debugger. Shutting the Application.");
        ret = false;
    }
    else 
    {
        NOUS_DEBUG("Vulkan Debugger created successfully!");
    }

    // Surface
    NOUS_DEBUG("Creating Vulkan surface...");
    if (!CreateSurface()) 
    {
        NOUS_ERROR("Failed to create Vulkan Surface. Shutting the Application.");
        ret = false;
    }
    else 
    {
        NOUS_DEBUG("Vulkan Surface created successfully!");
    }

    // Physical Device
    NOUS_DEBUG("Searching for a suitable Physical Device...");
    if (!PickPhysicalDevice(vkContext))
    {
        NOUS_ERROR("Failed to find a suitable GPU!");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Suitable GPU found!");
    }

    // Logical Device
    NOUS_DEBUG("Creating Vulkan Logical Device...");
    if (!CreateLogicalDevice(vkContext))
    {
        NOUS_ERROR("Failed to create Vulkan Logical Device. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Vulkan Logical Device created successfully!");
    }

    // Swap Chain
    NOUS_DEBUG("Creating Vulkan Swap Chain...");
    if (!CreateSwapChain(vkContext, vkContext->framebufferWidth, vkContext->framebufferHeight, &vkContext->swapChain))
    {
        NOUS_ERROR("Failed to create Vulkan Swap Chain. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Vulkan Swap Chain created successfully!");
    }

    // Render Pass
    NOUS_DEBUG("Creating Vulkan Render Pass...");
    if (!CreateRenderpass(vkContext, &vkContext->mainRenderpass,
        0, 0, vkContext->framebufferWidth, vkContext->framebufferHeight,
        0.0f, 0.0f, 0.2f, 1.0f,
        1.0f,
        0))
    {
        NOUS_ERROR("Failed to create Vulkan Render Pass. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Vulkan Render Pass created successfully!");
    }

    // Create Command Buffers
    NOUS_DEBUG("Creating Vulkan Command Buffers...");
    if (!NOUS_VulkanCommandBuffer::CreateCommandBuffers(vkContext))
    {
        NOUS_ERROR("Failed to create Vulkan Command Buffers. Shutting the Application.");
        ret = false;
    }
    else
    {
        NOUS_DEBUG("Vulkan Command Buffers created successfully!");
    }

	return ret;
}

void VulkanBackend::Shutdown()
{
    NOUS_VulkanCommandBuffer::DestroyCommandBuffers(vkContext);

    DestroyRenderpass(vkContext, &vkContext->mainRenderpass);

    DestroySwapChain(vkContext, &vkContext->swapChain);

    DestroyLogicalDevice(vkContext);

    if (enableValidationLayers) 
    {
        NOUS_DEBUG("Destroying Vulkan Debugger...");
        DestroyDebugUtilsMessengerEXT(vkContext->instance, vkContext->debugMessenger, vkContext->allocator);
    }

    NOUS_DEBUG("Destroying Vulkan Surface...");
    vkDestroySurfaceKHR(vkContext->instance, vkContext->surface, vkContext->allocator);

    NOUS_DEBUG("Destroying Vulkan Instance...");
    vkDestroyInstance(vkContext->instance, vkContext->allocator);
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

// ------------------------------------ Vulkan Pipeline Functions ------------------------------------ \\

bool VulkanBackend::CreateInstance()
{
    bool ret = true;

    if (enableValidationLayers && !CheckValidationLayerSupport(validationLayers)) 
    {
        NOUS_WARN("Vulkan Validation Layers requested, but not available!");
    }
    else 
    {
        NOUS_DEBUG("Vulkan Validation Layers enabled successfully!");
    }

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

    createInfo.enabledExtensionCount = static_cast<uint32>(extensions.GetLength());
    createInfo.ppEnabledExtensionNames = extensions.GetElements();

    createInfo.enabledLayerCount = enableValidationLayers ? static_cast<uint32>(validationLayers.size()) : 0;
    createInfo.ppEnabledLayerNames = enableValidationLayers ? validationLayers.data() : nullptr;

    // Validation Layers Loading Debugger
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) PopulateDebugMessengerCreateInfo(debugCreateInfo);
    createInfo.pNext = enableValidationLayers ? (VkDebugUtilsMessengerCreateInfoEXT*)&debugCreateInfo : nullptr;

    VK_CHECK_MSG(vkCreateInstance(&createInfo, vkContext->allocator, &vkContext->instance), "vkCreateInstance failed!");

    return ret;
}

bool VulkanBackend::SetupDebugMessenger()
{
    bool ret = true;

    if (!enableValidationLayers) return ret;

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

    PopulateDebugMessengerCreateInfo(debugCreateInfo);

    VK_CHECK_MSG(CreateDebugUtilsMessengerEXT(vkContext->instance, &debugCreateInfo, vkContext->allocator, &vkContext->debugMessenger) != VK_SUCCESS, "Failed to Set Up Debug Messenger!");

    return ret;
}

bool VulkanBackend::CreateSurface()
{
    return SDL_Vulkan_CreateSurface(GetSDLWindowData(), vkContext->instance, &vkContext->surface);
}

// ------------------------------------ Vulkan Helper Functions ------------------------------------ \\

bool VulkanBackend::CheckValidationLayerSupport(const std::vector<const char*>& validationLayers)
{
    bool ret = true;

    uint32 layerCount;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));

    DynamicArray<VkLayerProperties> availableLayers(layerCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.GetElements()));

    availableLayers.SetLength(layerCount);

    for (int i = 0; i < validationLayers.size(); ++i) 
    {
        const char* layerName = validationLayers[i];

        NOUS_DEBUG("Searching for Vulkan Validation Layer: %s...", layerName);

        bool layerFound = false;

        for (int j = 0; j < availableLayers.GetLength(); ++j) 
        {
            
            const auto& layerProperties = availableLayers[j];

            if (strcmp(layerName, layerProperties.layerName) == 0) 
            {
                NOUS_DEBUG("FOUND Vulkan Validation Layer: %s", layerName);
                layerFound = true;
                break;
            }

        }

        if (!layerFound) 
        {
            ret = false;
        }

    }

    return ret;
}

void VulkanBackend::ShowSupportedExtensions()
{
    uint32 extensionCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));

    DynamicArray<VkExtensionProperties> supportedExtensions(extensionCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, supportedExtensions.GetElements()));

    NOUS_DEBUG("Available Vulkan Extensions:\n");

    supportedExtensions.SetLength(extensionCount);

    for (int i = 0; i < supportedExtensions.GetLength(); ++i) 
    {
        NOUS_DEBUG("\t%s\n", supportedExtensions[i].extensionName);
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
        NOUS_DEBUG("\t%s\n", extensions[i]);
    }

    return extensions;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo, const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    PFN_vkCreateDebugUtilsMessengerEXT createDUMEXT =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

    if (createDUMEXT != nullptr) 
    {
        return createDUMEXT(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else 
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
    PFN_vkDestroyDebugUtilsMessengerEXT destroyDUMEXT =
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

    if (destroyDUMEXT != nullptr) 
    {
        destroyDUMEXT(instance, debugMessenger, pAllocator);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanBackend::DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity, VkDebugUtilsMessageTypeFlagsEXT messageType, const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    switch (messageSeverity)
    {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT: 
        {
            NOUS_TRACE("Validation layer: %s", pCallbackData->pMessage);
            break;
        }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
        {
            NOUS_INFO("Validation layer: %s", pCallbackData->pMessage);
            break;
        }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        {
            NOUS_WARN("Validation layer: %s", pCallbackData->pMessage);
            break;
        }
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
        {
            NOUS_ERROR("Validation layer: %s", pCallbackData->pMessage);
            break;
        }
    }

    return VK_FALSE;
}

void VulkanBackend::PopulateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& debugCreateInfo)
{
    debugCreateInfo = {};

    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;

    debugCreateInfo.messageSeverity = 
        /*VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT    |*/
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
        VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

    debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

    debugCreateInfo.pfnUserCallback = DebugCallback;
    debugCreateInfo.pUserData = nullptr;
}