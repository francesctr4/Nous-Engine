# ==============================
# Nous Engine - Renderer
# ==============================

set(RENDERER_SOURCES
        RendererTypes.h

        Frontend/RendererFrontend.cpp
        Frontend/RendererFrontend.h

        Backend/RendererBackend.cpp
        Backend/RendererBackend.h

        Backend/Vulkan/Shaders/BuiltIn/MaterialShader/VulkanMaterialShader.cpp
        Backend/Vulkan/Shaders/BuiltIn/MaterialShader/VulkanMaterialShader.h
        Backend/Vulkan/Shaders/VulkanShaderUtils.cpp
        Backend/Vulkan/Shaders/VulkanShaderUtils.h
        Backend/Vulkan/Shaders/BuiltIn/UIShader/VulkanUIShader.cpp
        Backend/Vulkan/Shaders/BuiltIn/UIShader/VulkanUIShader.h
        Backend/Vulkan/VulkanBackend.cpp
        Backend/Vulkan/VulkanBackend.h
        Backend/Vulkan/Resources/Buffer/VulkanBuffer.cpp
        Backend/Vulkan/Resources/Buffer/VulkanBuffer.h
        Backend/Vulkan/Rendering/CommandBuffer/VulkanCommandBuffer.cpp
        Backend/Vulkan/Rendering/CommandBuffer/VulkanCommandBuffer.h
        Backend/Vulkan/Core/DebugMessenger/VulkanDebugMessenger.cpp
        Backend/Vulkan/Core/DebugMessenger/VulkanDebugMessenger.h
        Backend/Vulkan/Core/Device/VulkanDevice.cpp
        Backend/Vulkan/Core/Device/VulkanDevice.h
        Backend/Vulkan/Rendering/Framebuffer/VulkanFramebuffer.cpp
        Backend/Vulkan/Rendering/Framebuffer/VulkanFramebuffer.h
        Backend/Vulkan/Resources/GraphicsPipeline/VulkanGraphicsPipeline.cpp
        Backend/Vulkan/Resources/GraphicsPipeline/VulkanGraphicsPipeline.h
        Backend/Vulkan/Resources/Image/VulkanImage.cpp
        Backend/Vulkan/Resources/Image/VulkanImage.h
        Backend/Vulkan/Resources/ImGui_Temp/VulkanImGuiResources.cpp
        Backend/Vulkan/Resources/ImGui_Temp/VulkanImGuiResources.h
        Backend/Vulkan/Core/Instance/VulkanInstance.cpp
        Backend/Vulkan/Core/Instance/VulkanInstance.h
        Backend/Vulkan/Rendering/CommandBuffer/VulkanMultithreading.cpp
        Backend/Vulkan/Rendering/CommandBuffer/VulkanMultithreading.h
        Backend/Vulkan/Rendering/Renderpass/VulkanRenderpass.cpp
        Backend/Vulkan/Rendering/Renderpass/VulkanRenderpass.h
        Backend/Vulkan/Core/Swapchain/VulkanSwapchain.cpp
        Backend/Vulkan/Core/Swapchain/VulkanSwapchain.h
        Backend/Vulkan/Rendering/SyncObjects/VulkanSyncObjects.cpp
        Backend/Vulkan/Rendering/SyncObjects/VulkanSyncObjects.h
        Backend/Vulkan/VulkanTypes.inl
        Backend/Vulkan/Utils/VulkanUtils.cpp
        Backend/Vulkan/Utils/VulkanUtils.h
        Frontend/IEditorOverlay.h
)

add_library(Renderer STATIC ${RENDERER_SOURCES})

set_target_properties(Renderer PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(Renderer PUBLIC ../..)

target_compile_definitions(Renderer PRIVATE NOUS_ENGINE_EXPORTS)

find_package(glm REQUIRED)
target_link_libraries(Renderer PUBLIC glm::glm)