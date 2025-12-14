# ==============================
# Nous Engine - Systems
# ==============================

add_subdirectory(CameraSystem)
add_subdirectory(ECS)
add_subdirectory(ResourceManager)
add_subdirectory(ShaderSystem)

add_library(Systems STATIC
        SystemManager.cpp
        SystemManager.h
)

set_target_properties(Systems PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(Systems PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(Systems PRIVATE
        NOUS_ENGINE_EXPORTS
)

target_link_libraries(Systems PUBLIC
        Camera
        ECS
        ResourceManager
        ShaderSystem
)