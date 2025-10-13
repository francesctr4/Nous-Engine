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
)

add_library(Core STATIC ${CORE_SOURCES})
add_library(NousEngine::Core ALIAS Core)

set_target_properties(Core PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

# Include parent for "Core/..."
target_include_directories(Core PUBLIC ../..)

# Add Modules subdirectory
add_subdirectory(Modules)

# Link Modules into Core (so any project linking Core automatically gets Modules)
target_link_libraries(Core PUBLIC NousEngine::Modules)