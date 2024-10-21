#include "VulkanDevice.h"

#include "Logger.h"
#include "MemoryManager.h"

bool IsPhysicalDeviceSuitable(VkPhysicalDevice& physicalDevice, VulkanContext* vkContext)
{
    bool ret = false;

    VkPhysicalDeviceRequirements requirements = {};

    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    VkPhysicalDeviceMemoryProperties deviceMemory;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemory);

    // --------------- Multisampling --------------- //
    VkSampleCountFlagBits msaaSamples = GetMaxUsableSampleCount(deviceProperties);

    VkPhysicalDeviceQueueFamilyIndices deviceIndices = FindQueueFamilies(physicalDevice, vkContext);
    requirements.queueFamilies = deviceIndices.IsComplete();

    requirements.extensionsSupported = CheckDeviceExtensionSupport(physicalDevice, vkContext);

    VkSwapChainSupportDetails swapChainSupport;

    if (requirements.extensionsSupported) 
    {
        swapChainSupport = QuerySwapChainSupport(physicalDevice, vkContext);
        requirements.swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }
    
    ret = deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
        deviceFeatures.geometryShader &&
        deviceFeatures.samplerAnisotropy &&
        deviceIndices.IsComplete() &&
        requirements.extensionsSupported &&
        requirements.swapChainAdequate;

    if (ret) 
    {
        vkContext->device.physicalDevice = physicalDevice;

        vkContext->device.swapChainSupport = swapChainSupport;
        vkContext->device.msaaSamples = msaaSamples;

        vkContext->device.graphicsQueueIndex = deviceIndices.graphicsFamilyIndex.value();
        vkContext->device.presentQueueIndex = deviceIndices.presentFamilyIndex.value();
        vkContext->device.computeQueueIndex = deviceIndices.computeFamilyIndex.value();
        vkContext->device.transferQueueIndex = deviceIndices.transferFamilyIndex.value();

        vkContext->device.properties = deviceProperties;
        vkContext->device.features = deviceFeatures;
        vkContext->device.memory = deviceMemory;
    }

    return ret;
}

VkPhysicalDeviceQueueFamilyIndices FindQueueFamilies(VkPhysicalDevice& physicalDevice, VulkanContext* vkContext)
{
    VkPhysicalDeviceQueueFamilyIndices indices;

    // Assign index to queue families that could be found

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

    for (int i = 0; i < queueFamilies.size(); ++i) 
    {
        if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) 
        {
            indices.graphicsFamilyIndex = i;
        }

        if (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)
        {
            indices.computeFamilyIndex = i;
        }

        if (queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT)
        {
            indices.transferFamilyIndex = i;
        }

        VkBool32 presentSupport = false;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, vkContext->surface, &presentSupport));

        if (presentSupport) 
        {
            indices.presentFamilyIndex = i;
        }

        if (indices.IsComplete()) 
        {
            break;
        }
    }

    return indices;
}

bool CheckDeviceExtensionSupport(VkPhysicalDevice& physicalDevice, VulkanContext* vkContext)
{
    uint32 extensionCount;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, nullptr));

    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    VK_CHECK(vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &extensionCount, availableExtensions.data()));

    std::unordered_set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

    for (const auto& extension : availableExtensions) 
    {
        requiredExtensions.erase(extension.extensionName);
    }

    return requiredExtensions.empty();
}

VkSwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice& physicalDevice, VulkanContext* vkContext)
{
    VkSwapChainSupportDetails details;

    // Device Formats

    uint32 formatCount;
    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, vkContext->surface, &formatCount, nullptr));

    if (formatCount != 0) 
    {
        details.formats.resize(formatCount);
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, vkContext->surface, &formatCount, details.formats.data()));
    }

    // Device Present Modes

    uint32 presentModeCount;
    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, vkContext->surface, &presentModeCount, nullptr));

    if (presentModeCount != 0) 
    {
        details.presentModes.resize(presentModeCount);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, vkContext->surface, &presentModeCount, details.presentModes.data()));
    }

    // Device Capabilities

    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, vkContext->surface, &details.capabilities));

    return details;
}

// --------------- Multisampling --------------- //
VkSampleCountFlagBits GetMaxUsableSampleCount(const VkPhysicalDeviceProperties& properties)
{
    VkSampleCountFlags counts = properties.limits.framebufferColorSampleCounts &
        properties.limits.framebufferDepthSampleCounts;

    if (counts & VK_SAMPLE_COUNT_64_BIT) {

        return VK_SAMPLE_COUNT_64_BIT;

    }
    else if (counts & VK_SAMPLE_COUNT_32_BIT) {

        return VK_SAMPLE_COUNT_32_BIT;

    }
    else if (counts & VK_SAMPLE_COUNT_16_BIT) {

        return VK_SAMPLE_COUNT_16_BIT;

    }
    else if (counts & VK_SAMPLE_COUNT_8_BIT) {

        return VK_SAMPLE_COUNT_8_BIT;

    }
    else if (counts & VK_SAMPLE_COUNT_4_BIT) {

        return VK_SAMPLE_COUNT_4_BIT;

    }
    else if (counts & VK_SAMPLE_COUNT_2_BIT) {

        return VK_SAMPLE_COUNT_2_BIT;

    }

    return VK_SAMPLE_COUNT_1_BIT;
}

void LogInfoAboutDevice(VulkanContext* vkContext)
{
    NOUS_INFO("Selected device: '%s'.", vkContext->device.properties.deviceName);

    switch (vkContext->device.properties.deviceType) 
    {
        default:
        case VK_PHYSICAL_DEVICE_TYPE_OTHER: 
        {
            NOUS_INFO("GPU type is Unknown.");
            break;
        }
        case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: 
        {
            NOUS_INFO("GPU type is Integrated.");
            break;
        }
        case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: 
        {
            NOUS_INFO("GPU type is Discrete.");
            break;
        }
        case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        {
            NOUS_INFO("GPU type is Virtual.");
            break;
        }
        case VK_PHYSICAL_DEVICE_TYPE_CPU: 
        {
            NOUS_INFO("GPU type is CPU.");
            break;
        }
    }

    NOUS_INFO(
        "GPU Driver version: %d.%d.%d",
        VK_VERSION_MAJOR(vkContext->device.properties.driverVersion),
        VK_VERSION_MINOR(vkContext->device.properties.driverVersion),
        VK_VERSION_PATCH(vkContext->device.properties.driverVersion));

    NOUS_INFO(
        "Vulkan API version: %d.%d.%d",
        VK_VERSION_MAJOR(vkContext->device.properties.apiVersion),
        VK_VERSION_MINOR(vkContext->device.properties.apiVersion),
        VK_VERSION_PATCH(vkContext->device.properties.apiVersion));

    // Memory information
    for (uint32 i = 0; i < vkContext->device.memory.memoryHeapCount; ++i) 
    {
        float32 memorySizeGib = ((static_cast<float32>(vkContext->device.memory.memoryHeaps[i].size)) / (1024.0f * 1024.0f * 1024.0f));

        if (vkContext->device.memory.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
        {
            NOUS_INFO("Local GPU memory: %.2f GiB", memorySizeGib);
        }
        else 
        {
            NOUS_INFO("Shared System memory: %.2f GiB", memorySizeGib);
        }
    }

    NOUS_INFO("Graphics | Present | Compute | Transfer | Name");
    // Print out some info about the device
    NOUS_INFO("       %d |       %d |       %d |        %d | %s",
        vkContext->device.graphicsQueueIndex != -1,
        vkContext->device.presentQueueIndex != -1,
        vkContext->device.computeQueueIndex != -1,
        vkContext->device.transferQueueIndex != -1,
        vkContext->device.properties.deviceName);

    NOUS_INFO("Device meets queue requirements.");
    NOUS_INFO("Graphics Family Index: %d", vkContext->device.graphicsQueueIndex);
    NOUS_INFO("Present Family Index:  %d", vkContext->device.presentQueueIndex);
    NOUS_INFO("Compute Family Index:  %d", vkContext->device.computeQueueIndex);
    NOUS_INFO("Transfer Family Index: %d", vkContext->device.transferQueueIndex);

    NOUS_INFO("MSAA: %d", static_cast<uint8>(vkContext->device.msaaSamples));
}