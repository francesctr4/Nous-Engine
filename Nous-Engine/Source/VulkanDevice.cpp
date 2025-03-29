#include "VulkanDevice.h"
#include "VulkanUtils.h"

#include "Logger.h"
#include "MemoryManager.h"

// ----------------------------------------------------------- //
// --------------------- Physical Device --------------------- //
// ----------------------------------------------------------- //

bool PickPhysicalDevice(VulkanContext* vkContext)
{
    bool ret = true;

    uint32 deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(vkContext->instance, &deviceCount, nullptr));

    if (deviceCount == 0)
    {
        NOUS_WARN("Failed to find GPUs with Vulkan support!");
        ret = false;
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(vkContext->instance, &deviceCount, devices.data()));

    for (int i = 0; i < devices.size(); ++i)
    {
        if (IsPhysicalDeviceSuitable(devices[i], vkContext))
        {
            LogInfoAboutDevice(vkContext);
            break;
        }
    }

    if (vkContext->device.physicalDevice == VK_NULL_HANDLE)
    {
        ret = false;
    }

    return ret;
}

bool IsPhysicalDeviceSuitable(VkPhysicalDevice& physicalDevice, VulkanContext* vkContext)
{
    bool ret = false;

    // Initialize a structure to track all our requirements, starting with everything set to false.
    // These will all need to be true for the device to be considered suitable.
    VkPhysicalDeviceRequirements requirements = {false};

    // Retrieve properties of the physical device, such as the type of device (integrated, discrete, etc.).
    VkPhysicalDeviceProperties deviceProperties;
    vkGetPhysicalDeviceProperties(physicalDevice, &deviceProperties);

    if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) 
    {
        requirements.discreteGPU = true;
    }

    // Retrieve the available features supported by the device.
    VkPhysicalDeviceFeatures deviceFeatures;
    vkGetPhysicalDeviceFeatures(physicalDevice, &deviceFeatures);

    requirements.geometryShader = deviceFeatures.geometryShader;
    requirements.samplerAnisotropy = deviceFeatures.samplerAnisotropy;

    // Retrieve the memory properties of the device (total memory, memory types, etc.).
    VkPhysicalDeviceMemoryProperties deviceMemory;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &deviceMemory);

    // Check if device supports local/host visible combo
    bool supportsDeviceLocalHostVisible = false;

    NOUS_INFO("Evaluating device: '%s'", deviceProperties.deviceName);

    for (uint32 i = 0; i < deviceMemory.memoryTypeCount; ++i)
    {
        // Check each memory type to see if its bit is set to 1.
        if (((deviceMemory.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) &&
            ((deviceMemory.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0))
        {
            supportsDeviceLocalHostVisible = true;
            break;
        }
    }

    // --------------- Multisampling --------------- //
    // Query the maximum usable sample count (MSAA level) supported by the device for anti-aliasing.
    VkSampleCountFlagBits msaaSamples = GetMaxUsableSampleCount(deviceProperties);

    // Check if the device has the necessary queue families (e.g., for graphics, compute, transfer operations).
    VkPhysicalDeviceQueueFamilyIndices deviceIndices = FindQueueFamilies(physicalDevice, vkContext);
    requirements.queueFamilies = deviceIndices.IsComplete();

    // Check if the required device extensions (like for swap chains) are supported by the device.
    requirements.extensionsSupported = CheckDeviceExtensionSupport(physicalDevice, vkContext);

    // If extensions are supported, check if swap chain support is adequate (i.e., available formats and present modes).
    VkSwapChainSupportDetails swapChainSupport;
    if (requirements.extensionsSupported) 
    {
        swapChainSupport = QuerySwapChainSupport(physicalDevice, vkContext);
        requirements.swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
    }

    // Check if all requirements are met by calling the Completed() method
    ret = requirements.Completed();

    if (ret) 
    {
        // If the device is suitable, store relevant device information in vkContext for future use.

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
        vkContext->device.supportsDeviceLocalHostVisible = supportsDeviceLocalHostVisible;
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

int32 FindMemoryIndex(VkPhysicalDevice& physicalDevice, uint32 typeFilter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

    for (uint32_t i = 0; i < memProperties.memoryTypeCount; ++i) 
    {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) 
        {
            return i;
        }

    }

    NOUS_WARN("Unable to find a suitable Memory Index!");
    return -1;
}

VkFormat FindDepthFormat(VkPhysicalDevice& physicalDevice)
{
    return FindSupportedFormat(physicalDevice, 
        { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
        VK_IMAGE_TILING_OPTIMAL,
        VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

VkFormat FindSupportedFormat(VkPhysicalDevice& physicalDevice, const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features)
{
    for (VkFormat format : candidates) 
    {
        VkFormatProperties properties;
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &properties);

        if (tiling == VK_IMAGE_TILING_LINEAR && (properties.linearTilingFeatures & features) == features) 
        {
            return format;
        }
        else if (tiling == VK_IMAGE_TILING_OPTIMAL && (properties.optimalTilingFeatures & features) == features) 
        {
            return format;
        }

    }

    NOUS_FATAL("Failed to find supported format!");
    return VK_FORMAT_UNDEFINED;
}

VkSampleCountFlagBits GetMaxUsableSampleCount(const VkPhysicalDeviceProperties& properties) // Multisampling
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
        float memorySizeGib = ((static_cast<float>(vkContext->device.memory.memoryHeaps[i].size)) / (1024.0f * 1024.0f * 1024.0f));

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

// ----------------------------------------------------------- //
// ---------------------- Logical Device --------------------- //
// ----------------------------------------------------------- //

bool CreateLogicalDevice(VulkanContext* vkContext)
{
    bool ret = true;

    VkPhysicalDeviceQueueFamilyIndices indices = FindQueueFamilies(vkContext->device.physicalDevice, vkContext);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;

    std::unordered_set<uint32> uniqueQueueFamilies =
    { indices.graphicsFamilyIndex.value(), indices.computeFamilyIndex.value(),
      indices.transferFamilyIndex.value(), indices.presentFamilyIndex.value() };

    float queuePriority = 1.0f;

    for (uint32 queueFamily : uniqueQueueFamilies)
    {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;

        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        queueCreateInfos.emplace_back(queueCreateInfo);
    }

    // Specifying used device features

    VkPhysicalDeviceFeatures deviceFeatures{};
    deviceFeatures.samplerAnisotropy = VK_TRUE;
    deviceFeatures.sampleRateShading = VK_TRUE; // Enable sample shading feature for the device.
    // [...]

    VkDeviceCreateInfo deviceCreateInfo{};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

    deviceCreateInfo.queueCreateInfoCount = static_cast<uint32>(queueCreateInfos.size());
    deviceCreateInfo.pQueueCreateInfos = queueCreateInfos.data();

    deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    deviceCreateInfo.enabledExtensionCount = static_cast<uint32>(deviceExtensions.size());
    deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions.data();

    /* DEPRECATED */
    //deviceCreateInfo.enabledLayerCount = enableValidationLayers ? static_cast<uint32>(validationLayers.size()) : 0;
    //deviceCreateInfo.ppEnabledLayerNames = enableValidationLayers ? validationLayers.data() : nullptr;

    VK_CHECK_MSG(vkCreateDevice(vkContext->device.physicalDevice, &deviceCreateInfo, vkContext->allocator, &vkContext->device.logicalDevice), "Failed vkCreateDevice!");

    vkGetDeviceQueue(vkContext->device.logicalDevice, indices.graphicsFamilyIndex.value(), 0, &vkContext->device.graphicsQueue);
    vkGetDeviceQueue(vkContext->device.logicalDevice, indices.presentFamilyIndex.value(), 0, &vkContext->device.presentQueue);
    vkGetDeviceQueue(vkContext->device.logicalDevice, indices.computeFamilyIndex.value(), 0, &vkContext->device.computeQueue);
    vkGetDeviceQueue(vkContext->device.logicalDevice, indices.transferFamilyIndex.value(), 0, &vkContext->device.transferQueue);

    NOUS_DEBUG("Logical Device Queues Obtained");

    CreateCommandPool(vkContext);

    return ret;
}

void CreateCommandPool(VulkanContext* vkContext)
{
    // Create Command Pool
    VkCommandPoolCreateInfo commandPoolCreateInfo{};
    commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;

    commandPoolCreateInfo.queueFamilyIndex = vkContext->device.graphicsQueueIndex;
    commandPoolCreateInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(vkContext->device.logicalDevice, &commandPoolCreateInfo,
        vkContext->allocator, &vkContext->device.graphicsCommandPool));

    NOUS_DEBUG("Graphics Command Pool Created");
}

void DestroyLogicalDevice(VulkanContext* vkContext)
{
    vkContext->device.graphicsQueue = 0;
    vkContext->device.presentQueue = 0;
    vkContext->device.computeQueue = 0;
    vkContext->device.transferQueue = 0;

    NOUS_DEBUG("Destroying Command Pool...");
    vkDestroyCommandPool(vkContext->device.logicalDevice, vkContext->device.graphicsCommandPool, vkContext->allocator);

    NOUS_DEBUG("Destroying Vulkan Logical Device...");
    vkDestroyDevice(vkContext->device.logicalDevice, vkContext->allocator);

    NOUS_DEBUG("Releasing Vulkan Physical Device...");
    vkContext->device.physicalDevice = 0;
}
