# ==============================
# Nous Engine - ECS
# ==============================

add_library(ResourceManager STATIC)

add_subdirectory(Builtins)
add_subdirectory(ImporterManager)
add_subdirectory(Resource)
add_subdirectory(ImportPipeline)
add_subdirectory(TypeRegistry)
add_subdirectory(ResourceTypes)
add_subdirectory(ScenePreloader)

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
)