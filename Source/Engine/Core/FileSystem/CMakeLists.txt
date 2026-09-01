set(CLASS_NAME "FileSystem")

# ==============================================================================
# FileSystem — converted to the public/private header layout (see CONVENTIONS.md).
#
#   include/FileSystem/**  is the API: <FileSystem/FileSystem.h>, <FileSystem/FileHandle.h>.
#   src/**                 is implementation, reachable only from inside this target.
#
# Both headers are public: FileHandle is the RAII file object every importer and
# the Logger's flush path use directly, so it is API, not detail.
# ==============================================================================

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# EngineCore (Globals.h / EngineExport.h / UpdateStatus.h) is named by this
# target's PUBLIC headers, so it rides on the _headers handle: consumers of
# those headers need the dir too. It is header-only, so this adds no linkage.
target_link_libraries(${CLASS_NAME}_headers INTERFACE EngineCore_headers)

# ReadAllBytes() returns NOUS_Vector<char>, named in the public FileHandle.h,
# so consumers of this header need the Utils include dir too.
target_link_libraries(${CLASS_NAME}_headers INTERFACE Utils_headers)

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
