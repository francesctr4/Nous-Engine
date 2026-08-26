add_library(PrefabManager STATIC
        include/PrefabManager.h
        src/PrefabManager.cpp
)

set_target_properties(PrefabManager PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(PrefabManager PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(PrefabManager PRIVATE NOUS_ENGINE_EXPORTS)

# ECS is PUBLIC and mutual: ECS spawns prefabs, PrefabManager reads components.
target_link_libraries(PrefabManager PUBLIC ECS)

target_link_libraries(PrefabManager PRIVATE
        Logger
        ResourceManager
        Utils
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
