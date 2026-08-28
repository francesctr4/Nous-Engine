set(CLASS_NAME "Logger")

# ==============================================================================
# Logger — converted to the public/private header layout (see CONVENTIONS.md).
#
#   include/Logger/**  is the API, reached as <Logger/Logger.h>.
#   src/**             is implementation, reachable only from inside this target.
#
# Logger has no private headers today; the src/ dir and the _private handle exist
# so the shape matches every other converted target and a future implementation
# header has an obvious home.
# ==============================================================================

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# EngineCore (Globals.h / EngineExport.h / UpdateStatus.h) is named by this
# target's PUBLIC headers, so it rides on the _headers handle: consumers of
# those headers need the dir too. It is header-only, so this adds no linkage.
target_link_libraries(${CLASS_NAME}_headers INTERFACE EngineCore_headers)

# An _headers handle must carry the header-level dependencies of the API it
# exposes, not just its own include dir: Asserts.h is public and includes
# <NOUS_Multithreading/NOUS_Multithreading.h>, so anyone given Logger's API dir
# alone (ModuleEditor, EditorUI) cannot compile it. This is the only such edge
# among the converted targets' public headers -- checked by grepping include/
# for cross-target includes -- so it introduces no INTERFACE cycle, which CMake
# would reject outright (unlike cycles between STATIC libraries).
target_link_libraries(${CLASS_NAME}_headers INTERFACE NOUS_Multithreading_headers)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(${CLASS_NAME} STATIC
        src/${CLASS_NAME}.cpp
)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/${CLASS_NAME}.h
        include/${CLASS_NAME}/Asserts.h
        include/${CLASS_NAME}/LogChannel.h
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

# NOUS_Multithreading is PUBLIC: Logger's async flush thread is part of its header
# surface (Asserts.h includes it). Note this closes a cycle -- FileSystem and
# MemoryManager both assert through Logger, and NOUS_Multithreading allocates
# through MemoryManager. CMake permits cycles between STATIC libraries; see the
# note in Source/Engine/CMakeLists.txt.
target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
        NOUS_Multithreading
)

target_link_libraries(${CLASS_NAME} PRIVATE
        FileSystem
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()

# The Core cycle (FileSystem <-> Logger <-> MemoryManager <-> NOUS_Multithreading)
# is intrinsic and documented in .claude/CLAUDE.md. CMake resolves static-library
# cycles by repeating the group on the link line, and its default of 2 is not
# enough here: FileSystem lands after MemoryManager in BOTH repetitions, so the
# last libFileSystem.a has no libMemoryManager.a after it. GNU ld scans archives
# left to right and never revisits one, so a symbol first needed from that last
# FileSystem object is unresolvable.
#
# Usually invisible: the reference comes from the NOUS_NEW_ARRAY<char> COMDAT
# template instantiation in FileHandle.cpp, which an ordinary build discards as
# unused. Any flag that keeps such sections alive (instrumentation, sanitizers,
# a different inlining decision) makes it a hard link error on Linux. Found
# 2026-08-29 while building with gcov instrumentation.
set_target_properties(${CLASS_NAME} PROPERTIES LINK_INTERFACE_MULTIPLICITY 3)
