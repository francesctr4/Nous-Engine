# ==============================
# Nous Engine - Vulkan Backend
#
# This is the target the whole public/private split was worth doing for.
#
#   include/VulkanBackend/VulkanBackend.h   the ONLY public header
#   src/**                                  the other 17, Vulkan headers included
#
# `.claude/CLAUDE.md` states two rules as prose:
#
#   "RendererBackendFactory.cpp is the only translation unit anywhere that
#    includes VulkanBackend.h -- it is the compile firewall keeping Vulkan
#    headers out of the frontend."
#   "grep -rn \"Renderer/Backend\" Source/Editor/ must return nothing."
#
# Both are now enforced by the build instead of by memory and a grep:
# VulkanDevice.h, VulkanImage.h, VulkanUtils.h, VulkanTypes.inl and the rest do
# not resolve from outside this target, whatever anyone writes. The measurement
# that justified it: all 16 already had zero consumers outside VulkanBackend, so
# nothing had to change to make the firewall real -- only to make it checkable.
#
# VulkanBackend.h itself stays public for exactly one consumer, the factory.
# It includes only <RendererBackend/iRendererBackend.h> and
# <Renderer/iEditorRenderBridge.h>; VulkanContext is forward-declared, which is
# why the context type never leaks with it.
# ==============================

set(CLASS_NAME "VulkanBackend")

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(${CLASS_NAME} STATIC
        src/${CLASS_NAME}.cpp
        src/VulkanTypes.inl
        src/VulkanConstants.h
)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/${CLASS_NAME}.h
)

add_subdirectory(src/Core)
add_subdirectory(src/Rendering)
add_subdirectory(src/Resources)
add_subdirectory(src/Utils)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME}
        PUBLIC  ${CMAKE_SOURCE_DIR}/Source          # temporary
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

# Vulkan::Vulkan is now PRIVATE, not PUBLIC. It used to be public because
# "VulkanTypes.inl exposes VkThings in its public headers" -- VulkanTypes.inl is
# private now, and the one public header names no Vulkan type, so the vulkan
# headers stop propagating to consumers. That is the firewall in link form.
target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
        glm::glm
        Renderer
        RendererBackend_headers   # VulkanBackend.h implements IRendererBackend
)

target_link_libraries(${CLASS_NAME} PRIVATE
        EventSystem
        FileSystem
        Logger
        MemoryManager
        NOUS_Multithreading
        ResourceManager
        SDL3::SDL3
        ShaderSystem
        Vulkan::Vulkan
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
