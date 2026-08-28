set(CLASS_NAME "FileSystem")

# ==============================================================================
# FileSystem — converted to the public/private header layout (see CONVENTIONS.md).
#
#   include/FileSystem/**  is the API: <FileSystem/FileSystem.h>, <FileSystem/FileHandle.h>.
#   src/**                 is implementation, reachable only from inside this target.
#
# Both headers are public: FileHandle is the RAII file object every importer and
# the Logger's flush path use directly, so it is API, not detail.
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
        include/${CLASS_NAME}/FileHandle.h
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

target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
)

target_link_libraries(${CLASS_NAME} PRIVATE
        Logger
        MemoryManager
)

add_subdirectory(src/FileHandle)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
