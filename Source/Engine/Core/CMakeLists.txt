# ==============================
# Nous Engine - Core
# ==============================

set(CORE_SOURCES
        Application.cpp
        Application.h
        Globals.h
        UpdateStatus.h

        "MemoryManager/MemoryManager.cpp"
        "MemoryManager/MemoryManager.h"
        "MemoryManager/CustomAllocators/DynamicAllocator.cpp"
        "MemoryManager/CustomAllocators/DynamicAllocator.h"
        "MemoryManager/CustomAllocators/FreeList.cpp"
        "MemoryManager/CustomAllocators/FreeList.h"
        "MemoryManager/CustomAllocators/LinearAllocator.cpp"
        "MemoryManager/CustomAllocators/LinearAllocator.h"
)

add_subdirectory(EventSystem)
add_subdirectory(FileSystem)
add_subdirectory(TimeManager)
add_subdirectory(Logger)

add_library(Core STATIC ${CORE_SOURCES})

set_target_properties(Core PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

# Include parent for "Core/..."
target_include_directories(Core PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(Core PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(Core PUBLIC
        EventSystem
        FileSystem
        TimeManager
        Logger
)
