# ==============================
# Nous Engine - ECS
# ==============================

add_library(ResourceManager STATIC)

add_subdirectory(Core)
add_subdirectory(Runtime)
add_subdirectory(Types)

set_target_properties(ResourceManager PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(ResourceManager PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(ResourceManager PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(ResourceManager PUBLIC
        glm::glm
        ShaderSystem
        FileWatcher       # HotReloader holds one
        MemoryManager     # NOUS_Vector in public resource headers
        Renderer          # IGPUResourceFactory / RendererTypes in public headers
)

# AudioSystem + VideoSystem close cycles: the Resource* types live here, the
# codecs and probes live there, and the importers call into them.
target_link_libraries(ResourceManager PRIVATE
        AudioSystem
        FileSystem
        Logger
        NOUS_Multithreading
        Utils
        VideoSystem
        assimp::assimp
)