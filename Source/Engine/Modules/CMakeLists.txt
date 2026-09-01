# ==============================
# Nous Engine - Modules
# ==============================

# ModuleBase owns only the Module base class. It is deliberately separate from the
# Modules aggregate below, which links every concrete module: the base class sits
# at the BOTTOM of the module graph (everyone depends on it) while the aggregate
# sits at the TOP (it depends on everyone). Holding both in one target made
# Modules <-> ModuleX a dependency cycle.
#
# Converted to the public/private header layout (see CONVENTIONS.md). The include
# prefix is the TARGET name, not the directory: <ModuleBase/Module.h>, even though
# the files live in Modules/. Naming it <Modules/Module.h> would collide with the
# Modules aggregate, which is an INTERFACE target with no headers of its own.
#
# Module.h is public -- all eight concrete modules and ModuleEditor derive from it
# in their own public headers.
add_library(ModuleBase_headers INTERFACE)
target_include_directories(ModuleBase_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# EngineCore (Globals.h / EngineExport.h / UpdateStatus.h) is named by this
# target's PUBLIC headers, so it rides on the _headers handle: consumers of
# those headers need the dir too. It is header-only, so this adds no linkage.
target_link_libraries(ModuleBase_headers INTERFACE EngineCore_headers)

add_library(ModuleBase_private INTERFACE)
target_include_directories(ModuleBase_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(ModuleBase STATIC
        src/Module.cpp
)

target_sources(ModuleBase PRIVATE
        include/ModuleBase/Module.h
)

set_target_properties(ModuleBase PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_compile_definitions(ModuleBase PRIVATE NOUS_ENGINE_EXPORTS)

target_include_directories(ModuleBase
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ModuleBase PUBLIC ModuleBase_headers)

add_subdirectory(ModuleWindow)
add_subdirectory(ModuleInput)
add_subdirectory(ModuleCamera3D)
add_subdirectory(ModuleResourceManager)
add_subdirectory(ModuleAudio)
add_subdirectory(ModuleVideo)
add_subdirectory(ModuleUI)
add_subdirectory(ModuleAI)
add_subdirectory(ModuleAnimation)
add_subdirectory(ModulePhysics)
add_subdirectory(ModuleParticles)
add_subdirectory(ModuleScene)
add_subdirectory(ModuleRenderer3D)

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
        ModuleWindow
        ModuleInput
        ModuleCamera3D
        ModuleResourceManager
        ModuleAudio
        ModuleVideo
        ModuleUI
        ModuleAI
        ModuleAnimation
        ModulePhysics
        ModuleParticles
        ModuleScene
        ModuleRenderer3D
)