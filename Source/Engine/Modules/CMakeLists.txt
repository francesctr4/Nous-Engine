# ==============================
# Nous Engine - Modules
# ==============================

# ModuleBase owns only the Module base class. It is deliberately separate from the
# Modules aggregate below, which links every concrete module: the base class sits
# at the BOTTOM of the module graph (everyone depends on it) while the aggregate
# sits at the TOP (it depends on everyone). Holding both in one target made
# Modules <-> ModuleX a dependency cycle. The files have not moved -- the include
# path is still "Engine/Modules/Module.h".
add_library(ModuleBase STATIC
        Module.h
        Module.cpp
)

set_target_properties(ModuleBase PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_compile_definitions(ModuleBase PRIVATE NOUS_ENGINE_EXPORTS)

target_include_directories(ModuleBase PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

add_subdirectory(ModuleCamera3D)
add_subdirectory(ModuleInput)
add_subdirectory(ModuleRenderer3D)
add_subdirectory(ModuleResourceManager)
add_subdirectory(ModuleScene)
add_subdirectory(ModuleWindow)
add_subdirectory(ModuleAudio)
add_subdirectory(ModuleVideo)

# Pure aggregate: no sources of its own, just a convenient handle on "all modules".
# INTERFACE rather than STATIC because it compiles nothing -- which also means the
# whole-archive walk in Source/Engine/CMakeLists.txt correctly skips it (it filters
# on STATIC_LIBRARY), while every concrete module it names is collected on its own.
add_library(Modules INTERFACE)

# =====================================================
# External dependencies
# =====================================================

target_link_libraries(Modules INTERFACE
        ModuleBase
        ModuleCamera3D
        ModuleInput
        ModuleRenderer3D
        ModuleResourceManager
        ModuleScene
        ModuleWindow
        ModuleAudio
        ModuleVideo
)