set(CLASS_NAME "ModuleParticles")

# ==============================================================================
# ModuleParticles -- public/private header layout (see CONVENTIONS.md).
#
#   include/ModuleParticles/**  is the API, reached as <ModuleParticles/ModuleParticles.h>.
#   src/**             is implementation, reachable only from inside this target.
#
# Skeleton module: no behaviour yet. The one public header is enough for
# Application to own the module and for the editor to reach it later.
# ==============================================================================

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# EngineCore (EngineExport.h / UpdateStatus.h) is named by this target's PUBLIC
# headers, so it rides on the _headers handle. Header-only: adds no linkage.
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

target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
        ModuleBase
        EventSystem_headers   # <Module>.h implements IEventListener
)

target_link_libraries(${CLASS_NAME} PRIVATE
        EventSystem
        Logger
)
