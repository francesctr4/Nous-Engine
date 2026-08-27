# ==============================
# Nous Engine - Renderer Frontend
# ==============================

add_library(RendererFrontend STATIC
        IEditorOverlay.h
        GroupGeometries.h
        GroupGeometries.cpp
        DynamicTextureCache.h
        DynamicTextureCache.cpp
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
        Renderer
        # RendererFrontend.h names ShaderLoaderTypes in its PUBLIC header, so the
        # API dir has to propagate. The ShaderSystem objects stay PRIVATE below --
        # this is headers only. Mis-scoped until the include/ split made it fail
        # to compile in ModuleEditor.
        ShaderSystem_headers
)

target_link_libraries(RendererFrontend PRIVATE
        CameraSystem
        Logger
        MemoryManager
        NOUS_Multithreading
        RendererBackend   # CreateRendererBackend -- the compile firewall
        ResourceManager
        ShaderSystem
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()