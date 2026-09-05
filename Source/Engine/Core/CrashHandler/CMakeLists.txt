set(CLASS_NAME "CrashHandler")

# ==============================================================================
# CrashHandler — public/private header layout (see CONVENTIONS.md).
#
#   include/CrashHandler/CrashHandler.h  is the API: one function, called from
#                                        MainEditor.cpp and MainGame.cpp.
#   src/**                               is implementation.
#
# It links NOTHING from the engine, and that is the point rather than an
# oversight: the handler must be installable as the first statement in main(),
# before InitializeMemory and InitializeLogging, and it must keep working when
# both of those are in an unusable state. Depending on Logger would make it
# useless exactly when it is needed -- the flush thread never runs in a dying
# process.
# ==============================================================================

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# EngineCore (EngineExport.h) is named by this target's PUBLIC header, so it
# rides on the _headers handle. Header-only, so it adds no linkage.
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

target_compile_definitions(${CLASS_NAME} PRIVATE
        NOUS_ENGINE_EXPORTS
)

target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
)

# dbghelp ships with the Windows SDK — MiniDumpWriteDump lives there. No new
# third-party dependency, and nothing to add to vcpkg.json.
if (WIN32)
    target_link_libraries(${CLASS_NAME} PRIVATE dbghelp)
endif()
