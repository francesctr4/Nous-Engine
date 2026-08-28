set(CLASS_NAME "FileWatcher")

# ==============================================================================
# FileWatcher — converted to the public/private header layout (see CONVENTIONS.md).
#
#   include/FileWatcher/**  is the API, reached as <FileWatcher/FileWatcher.h>.
#   src/**                  is implementation, reachable only from inside this target.
#
# One header, and it is public (ModuleRenderer3D owns a FileWatcher by value for
# shader hot-reload). No private headers today; the src/ dir and the _private
# handle exist so the shape matches every other converted target.
#
# ${CMAKE_SOURCE_DIR}/Source stays PUBLIC until the last target is converted.
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
        PUBLIC  ${CMAKE_SOURCE_DIR}/Source          # temporary, see note above
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE
        NOUS_ENGINE_EXPORTS
)

# Logger is PRIVATE: FileWatcher.h names nothing from it, only FileWatcher.cpp
# logs. The _headers handle therefore carries no dependency of its own.
target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
)

target_link_libraries(${CLASS_NAME} PRIVATE
        Logger
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
