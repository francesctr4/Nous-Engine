set(CLASS_NAME "MemoryManager")

# ==============================================================================
# MemoryManager — converted to the public/private header layout (CONVENTIONS.md).
#
#   include/MemoryManager/**  is the API, reached as <MemoryManager/...>.
#   src/**                    is implementation, unreachable from outside.
#
# The split here is the interesting part, and it was decided empirically by
# grepping who includes what from outside the target:
#
#   PUBLIC   MemoryManager.h        the NOUS_NEW/NOUS_DELETE facade (~80 consumers)
#            MemoryTag.h            the tag enum every allocation names
#            NOUS_STLAllocator.h    NOUS_Vector.h routes STL containers through it
#            FreeList.h             VulkanBuffer.cpp uses it for GPU sub-allocation
#
#   PRIVATE  DynamicAllocator.h     only MemoryManager.cpp constructs one
#            LinearAllocator.h      no consumer outside this target at all
#
# That last pair is what the layout buys: the pool allocators are now genuinely
# unreachable from the rest of the engine, so the facade is the only door.
# Their tests opt in through MemoryManager_private.
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
        include/${CLASS_NAME}/MemoryTag.h
        include/${CLASS_NAME}/FreeList.h
        include/${CLASS_NAME}/NOUS_STLAllocator.h
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
        magic_enum::magic_enum
)

if (ENABLE_PROFILING)
    find_package(Tracy CONFIG REQUIRED)
    target_link_libraries(${CLASS_NAME} PRIVATE Tracy::TracyClient)
endif()

add_subdirectory(src/CustomAllocators)

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
# Normally invisible: the reference comes from the NOUS_NEW_ARRAY<char> COMDAT
# template instantiation in FileHandle.cpp, which an ordinary build discards as
# unused. --coverage instruments and KEEPS it, which is how the coverage build
# found a fragility every other Linux config had been getting away with.
set_target_properties(${CLASS_NAME} PROPERTIES LINK_INTERFACE_MULTIPLICITY 3)
