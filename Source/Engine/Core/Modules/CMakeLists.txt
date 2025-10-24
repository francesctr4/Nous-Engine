# ==============================
# Nous Engine - Modules
# ==============================

set(MODULES_SOURCES
        ModuleCamera3D/ModuleCamera3D.cpp
        ModuleCamera3D/ModuleCamera3D.h
        ModuleFileSystem/ModuleFileSystem.cpp
        ModuleFileSystem/ModuleFileSystem.h
        ModuleInput/ModuleInput.cpp
        ModuleInput/ModuleInput.h
        ModuleRenderer3D/ModuleRenderer3D.cpp
        ModuleRenderer3D/ModuleRenderer3D.h
        ModuleResourceManager/ModuleResourceManager.cpp
        ModuleResourceManager/ModuleResourceManager.h
        ModuleScene/ModuleScene.cpp
        ModuleScene/ModuleScene.h
        ModuleWindow/ModuleWindow.cpp
        ModuleWindow/ModuleWindow.h
        Module.cpp
        Module.h
)

add_library(Modules STATIC ${MODULES_SOURCES})

set_target_properties(Modules PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

# Public include directory (so "Core/Modules/..." works)
target_include_directories(Modules PUBLIC ../../..)

target_compile_definitions(Modules PRIVATE NOUS_ENGINE_EXPORTS)

# =====================================================
# External dependencies
# =====================================================

find_package(SDL3 CONFIG REQUIRED)
target_link_libraries(Modules PUBLIC SDL3::SDL3)