# ==============================
# Nous Engine - Renderer
# ==============================

add_subdirectory(Frontend)
add_subdirectory(Backend)

add_library(Renderer STATIC
        RendererTypes.h
        RendererTypes.cpp
)

set_target_properties(Renderer PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(Renderer PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(Renderer PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(Renderer PUBLIC
        RendererFrontend
        RendererBackend
)