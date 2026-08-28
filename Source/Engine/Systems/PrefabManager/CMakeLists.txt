set(CLASS_NAME "PrefabManager")

# ==============================================================================
# PrefabManager - public/private header layout (see CONVENTIONS.md).
#
#   include/PrefabManager/**  is the API, reached as <PrefabManager/PrefabManager.h>.
#   src/**                    is implementation.
#
# Single-unit target, so include/ is flat and there is no sub-unit CMakeLists.
# One public header and no private one: everything this target does is the API.
# ==============================================================================

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# EngineCore (Globals.h / EngineExport.h / UpdateStatus.h) is named by this
# target's PUBLIC headers, so it rides on the _headers handle: consumers of
# those headers need the dir too. It is header-only, so this adds no linkage.
target_link_libraries(${CLASS_NAME}_headers INTERFACE EngineCore_headers)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(${CLASS_NAME} STATIC
        src/${CLASS_NAME}.cpp
)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/${CLASS_NAME}.h
)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

# ECS is PUBLIC and mutual: ECS spawns prefabs, PrefabManager reads components.
target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
        ECS
)

target_link_libraries(${CLASS_NAME} PRIVATE
        Logger
        ResourceManager
        Utils
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
