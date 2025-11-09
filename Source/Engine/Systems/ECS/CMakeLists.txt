# ==============================
# Nous Engine - ECS
# ==============================

add_library(ECS STATIC)

add_subdirectory(Scene)
add_subdirectory(GameObject)
add_subdirectory(Component)

set_target_properties(ECS PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(ECS PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(ECS PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(ECS PUBLIC
        glm::glm
)