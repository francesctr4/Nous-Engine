# ==============================
# Nous Engine - Vulkan Backend
# ==============================

add_library(VulkanBackend STATIC
        VulkanBackend.cpp
        VulkanBackend.h
        VulkanTypes.inl
)

add_subdirectory(Core)
add_subdirectory(Rendering)
add_subdirectory(Resources)
add_subdirectory(Utils)

set_target_properties(VulkanBackend PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(VulkanBackend PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(VulkanBackend PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(VulkanBackend PUBLIC
        glm::glm
)