# ==============================
# Nous Engine - Vulkan Backend
# ==============================

set(VULKAN_BACKEND_SOURCES
        Shaders/BuiltIn/MaterialShader/VulkanMaterialShader.cpp
        Shaders/BuiltIn/MaterialShader/VulkanMaterialShader.h
        Shaders/VulkanShaderUtils.cpp
        Shaders/VulkanShaderUtils.h
        Shaders/BuiltIn/UIShader/VulkanUIShader.cpp
        Shaders/BuiltIn/UIShader/VulkanUIShader.h
        VulkanBackend.cpp
        VulkanBackend.h
        Resources/Buffer/VulkanBuffer.cpp
        Resources/Buffer/VulkanBuffer.h
        Rendering/CommandBuffer/VulkanCommandBuffer.cpp
        Rendering/CommandBuffer/VulkanCommandBuffer.h
        Core/DebugMessenger/VulkanDebugMessenger.cpp
        Core/DebugMessenger/VulkanDebugMessenger.h
        Core/Device/VulkanDevice.cpp
        Core/Device/VulkanDevice.h
        Rendering/Framebuffer/VulkanFramebuffer.cpp
        Rendering/Framebuffer/VulkanFramebuffer.h
        Resources/GraphicsPipeline/VulkanGraphicsPipeline.cpp
        Resources/GraphicsPipeline/VulkanGraphicsPipeline.h
        Resources/Image/VulkanImage.cpp
        Resources/Image/VulkanImage.h
        Resources/ImGui_Temp/VulkanImGuiResources.cpp
        Resources/ImGui_Temp/VulkanImGuiResources.h
        Core/Instance/VulkanInstance.cpp
        Core/Instance/VulkanInstance.h
        Rendering/CommandBuffer/VulkanMultithreading.cpp
        Rendering/CommandBuffer/VulkanMultithreading.h
        Rendering/Renderpass/VulkanRenderpass.cpp
        Rendering/Renderpass/VulkanRenderpass.h
        Core/Swapchain/VulkanSwapchain.cpp
        Core/Swapchain/VulkanSwapchain.h
        Rendering/SyncObjects/VulkanSyncObjects.cpp
        Rendering/SyncObjects/VulkanSyncObjects.h
        VulkanTypes.inl
        Utils/VulkanUtils.cpp
        Utils/VulkanUtils.h
)

add_library(VulkanBackend STATIC ${VULKAN_BACKEND_SOURCES})

set_target_properties(VulkanBackend PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(VulkanBackend PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(VulkanBackend PRIVATE NOUS_ENGINE_EXPORTS)

find_package(glm REQUIRED)
target_link_libraries(VulkanBackend PUBLIC
        glm::glm
)