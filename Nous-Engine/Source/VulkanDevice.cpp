#include "VulkanDevice.h"

#include "Logger.h"
#include "MemoryManager.h"

bool IsPhysicalDeviceSuitable(VkPhysicalDevice physicalDevice, VulkanContext* vkContext)
{
    bool ret = false;

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    VkQueueFamilyIndices deviceIndices = FindQueueFamilies(physicalDevice, vkContext);

    bool extensionsSupported = CheckDeviceExtensionSupport(physicalDevice, vkContext);

    bool swapChainAdequate = false;

    if (extensionsSupported) {

        VkSwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(physicalDevice, vkContext);
        swapChainAdequate = !swapChainSupport.formats.IsEmpty() && !swapChainSupport.presentModes.IsEmpty();

    }

    ret = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
        deviceFeatures.geometryShader &&
        deviceFeatures.samplerAnisotropy &&
        deviceIndices.IsComplete() &&
        extensionsSupported &&
        swapChainAdequate;

    if (ret) 
    {
        std::cout << "Using GPU: ";
        std::cout << deviceProperties.deviceName << std::endl;
    }

    return ret;
}

VkQueueFamilyIndices FindQueueFamilies(VkPhysicalDevice physicalDevice, VulkanContext* vkContext)
{
    VkQueueFamilyIndices indices;

    // Assign index to queue families that could be found

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    DynamicArray<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.GetElements());

    queueFamilies.SetLength(queueFamilyCount);

    for (int i = 0; i < queueFamilies.GetLength(); ++i) 
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) 
        {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, vkContext->surface, &presentSupport);

        if (presentSupport) {

            indices.presentFamily = i;

        }

        if (indices.IsComplete()) {

            break;

        }
    }

    return indices;
}

bool CheckDeviceExtensionSupport(VkPhysicalDevice physicalDevice, VulkanContext* vkContext)
{
    uint32 extensionCount;
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr);

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data());

    std::unordered_set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) 
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

VkSwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice physicalDevice, VulkanContext* vkContext)
{
    VkSwapChainSupportDetails details;

    // Device Formats

    uint32 formatCount;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, vkContext->surface, &formatCount, nullptr));

    if (formatCount != 0) {

        details.formats.SetCapacity(formatCount);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, vkContext->surface, &formatCount, details.formats.GetElements()));
        details.formats.SetLength(formatCount);
    }

    // Device Present Modes

    uint32 presentModeCount;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, vkContext->surface, &presentModeCount, nullptr));

    if (presentModeCount != 0) {

        details.presentModes.SetCapacity(presentModeCount);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, vkContext->surface, &presentModeCount, details.presentModes.GetElements()));
        details.presentModes.SetLength(presentModeCount);
    }

    // Device Capabilities

    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, vkContext->surface, &details.capabilities));

    return details;
}

void LogInfoAboutDevice(VulkanContext* vkContext)
{
    std::cout << "Using GPU: ";
    std::cout << vkContext->device.properties.deviceName << std::endl;

    NOUS_INFO(
        "GPU Driver version: %d.%d.%d",
        VK_VERSION_MAJOR(vkContext->device.properties.driverVersion),
        VK_VERSION_MINOR(vkContext->device.properties.driverVersion),
        VK_VERSION_PATCH(vkContext->device.properties.driverVersion));
    // Vulkan API version.
    NOUS_INFO(
        "Vulkan API version: %d.%d.%d",
        VK_VERSION_MAJOR(vkContext->device.properties.apiVersion),
        VK_VERSION_MINOR(vkContext->device.properties.apiVersion),
        VK_VERSION_PATCH(vkContext->device.properties.apiVersion));
}
