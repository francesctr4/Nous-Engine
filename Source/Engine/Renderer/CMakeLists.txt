# ==============================
# Nous Engine - Renderer
# ==============================

add_subdirectory(Backend)
add_subdirectory(Frontend)

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

# Renderer is the SHARED-TYPES target, not an aggregate: it compiles RendererTypes
# and owns the four interface headers (iRendererBackend, iRenderWindow,
# iRenderResourceProvider, iEditorRenderBridge, IGPUResourceFactory) that the
# frontend, the backend and their consumers all depend on. It therefore sits at the
# BOTTOM of the renderer graph and must NOT link RendererFrontend/RendererBackend --
# doing so made Renderer <-> RendererFrontend <-> RendererBackend a cycle, for the
# same reason Modules <-> ModuleX was one. Consumers link the half they use;
# nothing outside Source/Engine depended on the transitive pull.
target_link_libraries(Renderer PUBLIC
        glm::glm          # RendererTypes.h is glm-typed throughout
)