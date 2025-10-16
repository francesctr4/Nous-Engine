# ==============================
# Nous Engine - Renderer
# ==============================

set(RENDERER_SOURCES
        RendererTypes.h

        Frontend/RendererFrontend.cpp
        Frontend/RendererFrontend.h

        Backend/RendererBackend.cpp
        Backend/RendererBackend.h

        Backend/Vulkan/Shaders/VulkanMaterialShader.cpp
        Backend/Vulkan/Shaders/VulkanMaterialShader.h
        Backend/Vulkan/Shaders/VulkanShaderUtils.cpp
        Backend/Vulkan/Shaders/VulkanShaderUtils.h
        Backend/Vulkan/Shaders/VulkanUIShader.cpp
        Backend/Vulkan/Shaders/VulkanUIShader.h
        Backend/Vulkan/VulkanBackend.cpp
        Backend/Vulkan/VulkanBackend.h
        Backend/Vulkan/VulkanBuffer.cpp
        Backend/Vulkan/VulkanBuffer.h
        Backend/Vulkan/VulkanCommandBuffer.cpp
        Backend/Vulkan/VulkanCommandBuffer.h
        Backend/Vulkan/VulkanDebugMessenger.cpp
        Backend/Vulkan/VulkanDebugMessenger.h
        Backend/Vulkan/VulkanDevice.cpp
        Backend/Vulkan/VulkanDevice.h
        Backend/Vulkan/VulkanExternal.cpp
        Backend/Vulkan/VulkanExternal.h
        Backend/Vulkan/VulkanFramebuffer.cpp
        Backend/Vulkan/VulkanFramebuffer.h
        Backend/Vulkan/VulkanGlobals.h
        Backend/Vulkan/VulkanGraphicsPipeline.cpp
        Backend/Vulkan/VulkanGraphicsPipeline.h
        Backend/Vulkan/VulkanImage.cpp
        Backend/Vulkan/VulkanImage.h
        Backend/Vulkan/VulkanImGuiResources.cpp
        Backend/Vulkan/VulkanImGuiResources.h
        Backend/Vulkan/VulkanInstance.cpp
        Backend/Vulkan/VulkanInstance.h
        Backend/Vulkan/VulkanMultithreading.cpp
        Backend/Vulkan/VulkanMultithreading.h
        Backend/Vulkan/VulkanRenderpass.cpp
        Backend/Vulkan/VulkanRenderpass.h
        Backend/Vulkan/VulkanSwapchain.cpp
        Backend/Vulkan/VulkanSwapchain.h
        Backend/Vulkan/VulkanSyncObjects.cpp
        Backend/Vulkan/VulkanSyncObjects.h
        Backend/Vulkan/VulkanTypes.inl
        Backend/Vulkan/VulkanUtils.cpp
        Backend/Vulkan/VulkanUtils.h
        Frontend/IEditorOverlay.h
)

add_library(Renderer OBJECT ${RENDERER_SOURCES})

set_target_properties(Renderer PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(Renderer PUBLIC ../..)

find_package(glm REQUIRED)
target_link_libraries(Renderer PUBLIC glm::glm)