set(CLASS_NAME "ModuleRenderer3D")

add_library(${CLASS_NAME} STATIC
        include/${CLASS_NAME}.h
        src/${CLASS_NAME}.cpp
)

target_include_directories(${CLASS_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(${CLASS_NAME} PUBLIC
        SDL3::SDL3
        ModuleBase
        FileWatcher       # owns the shader hot-reload watcher
        Renderer
)

target_link_libraries(${CLASS_NAME} PRIVATE
        Camera
        ECS
        EventSystem
        FileSystem
        Logger
        MemoryManager
        ModuleCamera3D
        ModuleResourceManager
        ModuleScene
        ModuleWindow
        RendererFrontend
        ResourceManager
        Utils
        glm::glm
)
