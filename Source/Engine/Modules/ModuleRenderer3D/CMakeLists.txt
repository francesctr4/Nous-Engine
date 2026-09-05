set(CLASS_NAME "ModuleRenderer3D")

# ==============================================================================
# ModuleRenderer3D -- converted to the public/private header layout (see CONVENTIONS.md).
#
#   include/ModuleRenderer3D/**  is the API, reached as <ModuleRenderer3D/ModuleRenderer3D.h>.
#   src/**                  is implementation, reachable only from inside this target.
#
# The one header is public: Application owns this module by value and the
# editor reaches it through EditorContext. No private headers today, so the
# src/ dir and the _private handle exist only to match every other target.
# ==============================================================================

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# EngineCore (Globals.h / EngineExport.h / UpdateStatus.h) is named by this
# target's PUBLIC headers, so it rides on the _headers handle: consumers of
# those headers need the dir too. It is header-only, so this adds no linkage.
target_link_libraries(${CLASS_NAME}_headers INTERFACE EngineCore_headers)

# SkinningPairing.h names GeometryRenderData, so the Renderer API dir rides on the
# _headers handle too. Headers only -- this adds no linkage.
target_link_libraries(${CLASS_NAME}_headers INTERFACE Renderer_headers)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(${CLASS_NAME} STATIC
        src/${CLASS_NAME}.cpp
)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/${CLASS_NAME}.h
        include/${CLASS_NAME}/SkinningPairing.h
)

target_include_directories(${CLASS_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
        SDL3::SDL3
        ModuleBase
        FileWatcher       # owns the shader hot-reload watcher
        Renderer
        EventSystem_headers   # <Module>.h implements IEventListener
        # ModuleRenderer3D.h holds a vector<Vertex3D> by value for the normals
        # overlay, and RendererTypes.h only forward-declares Vertex3D.
        Utils_headers
)

target_link_libraries(${CLASS_NAME} PRIVATE
        CameraSystem
        ECS
        EventSystem
        FileSystem
        Logger
        MemoryManager
        ModuleCamera3D
        ModuleScene
        ModuleWindow
        RendererFrontend
        ResourceManager   # IResourceGpuSync / IResourceLoader; the module itself is no longer named
        Utils
        glm::glm
        Scripting_headers
)


if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
