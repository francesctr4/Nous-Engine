# ==============================
# Nous Engine - Renderer Backend
# ==============================

add_subdirectory(Vulkan)

add_library(RendererBackend STATIC
        RendererBackend.h
        RendererBackend.cpp
)

set_target_properties(RendererBackend PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(RendererBackend PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(RendererBackend PRIVATE
        NOUS_ENGINE_EXPORTS
)

target_link_libraries(RendererBackend PUBLIC
        glm::glm
        VulkanBackend
)