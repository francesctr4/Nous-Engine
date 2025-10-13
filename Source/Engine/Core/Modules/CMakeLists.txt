# ==============================
# Nous Engine - Modules
# ==============================

set(MODULES_SOURCES
        ModuleCamera3D.cpp
        ModuleCamera3D.h
        ModuleEditor.cpp
        ModuleEditor.h
        ModuleFileSystem.cpp
        ModuleFileSystem.h
        ModuleInput.cpp
        ModuleInput.h
        ModuleRenderer3D.cpp
        ModuleRenderer3D.h
        ModuleResourceManager.cpp
        ModuleResourceManager.h
        ModuleScene.cpp
        ModuleScene.h
        ModuleWindow.cpp
        ModuleWindow.h
)

add_library(Modules STATIC ${MODULES_SOURCES})
add_library(NousEngine::Modules ALIAS Modules)

set_target_properties(Modules PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

# Public include directory (so "Core/Modules/..." works)
target_include_directories(Modules PUBLIC ../../..)

# =====================================================
# External dependencies
# =====================================================

find_package(SDL3 CONFIG REQUIRED)
target_link_libraries(Modules PUBLIC SDL3::SDL3)