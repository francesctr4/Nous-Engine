#include "VulkanInstance.h"
#include <EngineCore/AppConfig.h>
#include "Utils/VulkanUtils.h"
#include "Core/DebugMessenger/VulkanDebugMessenger.h"

#include "SDL3/SDL.h"
#include "SDL3/SDL_vulkan.h"

#include <Renderer/iRenderWindow.h>
#include <cstddef>
#include <cstring>

bool NOUS_VulkanInstance::CreateInstance(VulkanContext* vkContext)
{
    bool ret = true;

    // Only enable validation if it was requested AND the layers are actually present;
    // otherwise vkCreateInstance would fail with VK_ERROR_LAYER_NOT_PRESENT.
    bool useValidation = enableValidationLayers && CheckValidationLayerSupport(validationLayers);

    if (enableValidationLayers)
    {
        if (useValidation)
        {
            NOUS_DEBUG("Vulkan Validation Layers enabled successfully!");
        }
        else
        {
            NOUS_WARN("Vulkan Validation Layers requested, but not available! Continuing without them.");
        }
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

    // Query the highest instance version the loader supports, then clamp our requested
    // API version to it so we fail gracefully on a 1.0/1.1-only loader instead of relying
    // on driver quirks. vkEnumerateInstanceVersion is a 1.1 entry point: look it up via
    // vkGetInstanceProcAddr and fall back to 1.0 when it isn't exported.
    uint32_t loaderVersion = VK_API_VERSION_1_0;
    auto fpEnumerateInstanceVersion = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(
        vkGetInstanceProcAddr(nullptr, "vkEnumerateInstanceVersion"));
    if (fpEnumerateInstanceVersion != nullptr)
    {
        VK_CHECK(fpEnumerateInstanceVersion(&loaderVersion));
    }

    const uint32_t requestedVersion = VK_API_VERSION_1_2;
    const uint32_t selectedVersion = (loaderVersion < requestedVersion) ? loaderVersion : requestedVersion;

    NOUS_INFO("Vulkan API Version: using %u.%u.%u (loader supports up to %u.%u.%u)",
        VK_API_VERSION_MAJOR(selectedVersion), VK_API_VERSION_MINOR(selectedVersion), VK_API_VERSION_PATCH(selectedVersion),
        VK_API_VERSION_MAJOR(loaderVersion), VK_API_VERSION_MINOR(loaderVersion), VK_API_VERSION_PATCH(loaderVersion));

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = selectedVersion;
    appInfo.pApplicationName = TITLE;
    appInfo.applicationVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);
    appInfo.pEngineName = TITLE;
    appInfo.engineVersion = VK_MAKE_API_VERSION(0, 1, 0, 0);

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

    // Conditionally enable portability enumeration
    if (requiresPortability) 
    {
        createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    }

    // Set up debug messenger create info so validation messages emitted *during*
    // vkCreateInstance / vkDestroyInstance are captured (before the real messenger exists).
    // It is chained into VkInstanceCreateInfo::pNext and must outlive the call below.
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

    // Opt into synchronization validation (semaphore/fence/barrier hazards) on top of the
    // standard validation layer. Requires VK_EXT_validation_features (added in GetRequiredExtensions).
    const std::array<VkValidationFeatureEnableEXT, 1> enabledValidationFeatures =
    {
        VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
    };

    VkValidationFeaturesEXT validationFeatures{};
    validationFeatures.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;

    if (useValidation)
    {
        NOUS_VulkanDebugMessenger::PopulateDebugMessengerCreateInfo(debugCreateInfo);

        validationFeatures.enabledValidationFeatureCount = static_cast<uint32_t>(enabledValidationFeatures.size());
        validationFeatures.pEnabledValidationFeatures = enabledValidationFeatures.data();
        // Chain: createInfo -> validationFeatures -> debugCreateInfo (so creation-time messages
        // still reach the callback).
        validationFeatures.pNext = &debugCreateInfo;
    }

    createInfo.pNext = useValidation ? static_cast<const void*>(&validationFeatures) : nullptr;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = useValidation ? static_cast<uint32_t>(validationLayers.size()) : 0;
    createInfo.ppEnabledLayerNames = useValidation ? validationLayers.data() : nullptr;

    VkResult result = vkCreateInstance(&createInfo, vkContext->allocator, &vkContext->instance);

    if (result != VK_SUCCESS) 
    {
        NOUS_ERROR("Failed to create Vulkan instance: %s", VkResultMessage(result, true).c_str());
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
    return SDL_Vulkan_CreateSurface(vkContext->window->GetSDL_Window(), vkContext->instance, vkContext->allocator, &vkContext->surface);
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

    uint32_t layerCount;
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
    uint32_t extensionCount = 0;
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
    uint32_t sdlExtensionCount = 0;  // Use unsigned int to match SDL3 API

    // Get the array of required instance extensions from SDL
    const char* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    if (!sdlExtensions)
    {
        NOUS_ERROR("Could not get the required instance extensions from SDL.");
        return {};  // Return empty vector on error
    }

    // Create a vector and copy the extensions
    std::vector<const char*> extensions;
    extensions.reserve(sdlExtensionCount + (enableValidationLayers ? 1 : 0));  // Preallocate memory

    // Copy SDL extensions into our vector
    for (uint32_t i = 0; i < sdlExtensionCount; ++i)
    {
        extensions.push_back(sdlExtensions[i]);
    }

#ifdef __APPLE__
    // MoltenVK requires portability enumeration so physical devices are visible.
    // The flag VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR is set automatically
    // in CreateInstance() when this extension is detected in the list.
    extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
#endif

    // Add debug utils + validation features extensions if validation is enabled.
    // VK_EXT_validation_features is required to pass VkValidationFeaturesEXT (synchronization
    // validation) in the instance pNext chain.
    if (enableValidationLayers)
    {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        extensions.push_back(VK_EXT_VALIDATION_FEATURES_EXTENSION_NAME);
    }

    // Debug output
    NOUS_DEBUG("Required Vulkan Extensions:\n");
    for (size_t i = 0; i < extensions.size(); ++i)  // Use size_t for vector size
    {
        NOUS_DEBUG("\t%s\n", extensions[i]);
    }

    return extensions;
}