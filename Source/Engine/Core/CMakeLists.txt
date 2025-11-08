# ==============================
# Nous Engine - Core
# ==============================

set(CORE_SOURCES
        Application.cpp
        Application.h
        Globals.h
        UpdateStatus.h

        "Time Management/TimeManager.h"
        "Time Management/Timer.cpp"
        "Time Management/Timer.h"
        "Event System/IEventListener.h"
        "Event System/Event.h"
        "Event System/EventSystem.cpp"
        "Event System/Event.cpp"

        "Logging System/Asserts.h"
        "Logging System/Logger.cpp"
        "Logging System/Logger.h"
        "Logging System/LogChannel.h"

        "Event System/EventSystem.h"

        "File System/FileHandle.cpp"
        "File System/FileHandle.h"
        "File System/FileManager.cpp"
        "File System/FileManager.h"

        "Memory Manager/MemoryManager.cpp"
        "Memory Manager/MemoryManager.h"

        "Memory Manager/Custom Allocators/DynamicAllocator.cpp"
        "Memory Manager/Custom Allocators/DynamicAllocator.h"
        "Memory Manager/Custom Allocators/FreeList.cpp"
        "Memory Manager/Custom Allocators/FreeList.h"
        "Memory Manager/Custom Allocators/LinearAllocator.cpp"
        "Memory Manager/Custom Allocators/LinearAllocator.h"
)

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

target_link_libraries(Core
        PUBLIC
        glm::glm            # 👈 exported publicly
)
