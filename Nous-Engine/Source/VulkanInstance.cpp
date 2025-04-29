#include "VulkanInstance.h"
#include "VulkanUtils.h"
#include "VulkanDebugMessenger.h"
#include "VulkanExternal.h"
#include "VulkanGlobals.h"

#include "SDL2.h"

bool NOUS_VulkanInstance::CreateInstance(VulkanContext* vkContext)
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

    std::vector<const char*> extensions = GetRequiredExtensions();

    // Add portability enumeration extension if needed
    bool requiresPortability = false;
    for (auto& ext : extensions) 
    {
        if (strcmp(ext, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME) == 0) 
        {
            requiresPortability = true;
            break;
        }
    }

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_2;
    appInfo.pApplicationName = TITLE;
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = TITLE;
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    // Conditionally enable portability enumeration
    if (requiresPortability) 
    {
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    // Set up validation features
    std::array<VkValidationFeatureEnableEXT, 2> enabledFeatures = 
    {
        VK_VALIDATION_FEATURE_ENABLE_GPU_ASSISTED_EXT,
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
    };

    VkValidationFeaturesEXT validationFeatures{};
    validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
    //validationFeatures.enabledValidationFeatureCount = static_cast<uint32>(enabledFeatures.size());
    //validationFeatures.pEnabledValidationFeatures = enabledFeatures.data();
    validationFeatures.enabledValidationFeatureCount = 0;
    validationFeatures.pEnabledValidationFeatures = nullptr;

    // Set up debug messenger create info
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (enableValidationLayers) 
    {
        NOUS_VulkanDebugMessenger::PopulateDebugMessengerCreateInfo(debugCreateInfo);
        validationFeatures.pNext = &debugCreateInfo;
    }

    // Chain the validation features to the main create info
    createInfo.pNext = &validationFeatures;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = enableValidationLayers ?
        static_cast<uint32>(validationLayers.size()) : 0;
    createInfo.ppEnabledLayerNames = enableValidationLayers ?
        validationLayers.data() : nullptr;

    createInfo.enabledLayerCount = enableValidationLayers ? static_cast<uint32>(validationLayers.size()) : 0;
    createInfo.ppEnabledLayerNames = enableValidationLayers ? validationLayers.data() : nullptr;

    VkResult result = vkCreateInstance(&createInfo, vkContext->allocator, &vkContext->instance);

    if (result != VK_SUCCESS) 
    {
        NOUS_ERROR("Failed to create Vulkan instance: %s", VkResultMessage(result, true));
        ret = false;
    }

    return ret;
}

void NOUS_VulkanInstance::DestroyInstance(VulkanContext* vkContext)
{
    NOUS_DEBUG("Destroying Vulkan Instance...");
    vkDestroyInstance(vkContext->instance, vkContext->allocator);
}

bool NOUS_VulkanInstance::CreateSurface(VulkanContext* vkContext)
{
    return SDL_Vulkan_CreateSurface(GetSDLWindowData(), vkContext->instance, &vkContext->surface);
}

void NOUS_VulkanInstance::DestroySurface(VulkanContext* vkContext)
{
    NOUS_DEBUG("Destroying Vulkan Surface...");
    vkDestroySurfaceKHR(vkContext->instance, vkContext->surface, vkContext->allocator);
}

// ------------------------------------ Vulkan Helper Functions ------------------------------------ \\

bool NOUS_VulkanInstance::CheckValidationLayerSupport(const std::array<const char*, c_VALIDATION_LAYERS_COUNT>& validationLayers)
{
    bool ret = true;

    uint32 layerCount;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, nullptr));

    std::vector<VkLayerProperties> availableLayers(layerCount);
    VK_CHECK(vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()));

    availableLayers.resize(layerCount);

    for (int i = 0; i < validationLayers.size(); ++i)
    {
        const char* layerName = validationLayers[i];

        NOUS_DEBUG("Searching for Vulkan Validation Layer: %s...", layerName);

        bool layerFound = false;

        for (int j = 0; j < availableLayers.size(); ++j)
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

void NOUS_VulkanInstance::ShowSupportedExtensions()
{
    uint32 extensionCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr));

    std::vector<VkExtensionProperties> supportedExtensions(extensionCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, supportedExtensions.data()));

    NOUS_DEBUG("Available Vulkan Extensions:\n");

    supportedExtensions.resize(extensionCount);

    for (int i = 0; i < supportedExtensions.size(); ++i)
    {
        NOUS_DEBUG("\t%s\n", supportedExtensions[i].extensionName);
    }
}

std::vector<const char*> NOUS_VulkanInstance::GetRequiredExtensions()
{
    uint32 sdlExtensionCount = 0;

    if (!SDL_Vulkan_GetInstanceExtensions(GetSDLWindowData(), &sdlExtensionCount, nullptr))
    {
        NOUS_ERROR("Could not get the number of required instance extensions from SDL.");

    }

    std::vector<const char*> extensions(sdlExtensionCount);

    if (!SDL_Vulkan_GetInstanceExtensions(GetSDLWindowData(), &sdlExtensionCount, extensions.data()))
    {
        NOUS_ERROR("Could not get the name of required instance extensions from SDL.");

    }

    extensions.resize(sdlExtensionCount);

    // BUG FIX: "RenderDoc does not support requested instance extension: VK_KHR_portability_enumeration"
    //extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);

    if (enableValidationLayers)
    {
        extensions.emplace_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }

    NOUS_DEBUG("Required Vulkan Extensions:\n");

    for (int i = 0; i < extensions.size(); ++i)
    {
        NOUS_DEBUG("\t%s\n", extensions[i]);
    }

    return extensions;
}