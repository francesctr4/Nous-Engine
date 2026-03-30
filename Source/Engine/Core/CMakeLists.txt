# ==============================
# Nous Engine - Core
# ==============================

add_subdirectory(EventSystem)
add_subdirectory(FileSystem)
add_subdirectory(FileWatcher)
add_subdirectory(Logger)
add_subdirectory(MemoryManager)
add_subdirectory(TimeManager)

add_library(Core STATIC
        Application.cpp
        Application.h
        Globals.h
        UpdateStatus.h
)

set_target_properties(Core PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(Core PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(Core PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(Core PUBLIC
        EventSystem
        FileSystem
        FileWatcher
        TimeManager
        Logger
        MemoryManager
)
