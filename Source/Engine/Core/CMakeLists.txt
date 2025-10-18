# ==============================
# Nous Engine - Core
# ==============================

set(CORE_SOURCES
        Application.cpp
        Application.h
        Globals.h
        UpdateStatus.h
        Module.cpp
        Module.h
        EngineExport.h
)

add_library(Core OBJECT ${CORE_SOURCES})

set_target_properties(Core PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

# Include parent for "Core/..."
target_include_directories(Core PUBLIC ../..)

target_compile_definitions(Core PRIVATE NOUS_ENGINE_EXPORTS)

# Add Modules subdirectory
add_subdirectory(Modules)

# Link Modules into Core (so any project linking Core automatically gets Modules)
target_link_libraries(Core PUBLIC Modules)
