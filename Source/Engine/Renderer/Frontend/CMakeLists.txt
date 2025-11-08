# ==============================
# Nous Engine - Renderer Frontend
# ==============================

add_library(RendererFrontend STATIC
        IEditorOverlay.h
        RendererFrontend.h
        RendererFrontend.cpp
)

set_target_properties(RendererFrontend PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(RendererFrontend PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(RendererFrontend PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(RendererFrontend PUBLIC
        glm::glm
)