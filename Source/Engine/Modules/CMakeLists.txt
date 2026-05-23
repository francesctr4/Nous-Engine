# ==============================
# Nous Engine - Modules
# ==============================

add_subdirectory(ModuleCamera3D)
add_subdirectory(ModuleInput)
add_subdirectory(ModuleRenderer3D)
add_subdirectory(ModuleResourceManager)
add_subdirectory(ModuleScene)
add_subdirectory(ModuleWindow)

add_library(Modules STATIC
        Module.h
        Module.cpp
)

set_target_properties(Modules PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_compile_definitions(Modules PRIVATE NOUS_ENGINE_EXPORTS)

target_include_directories(Modules PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

# =====================================================
# External dependencies
# =====================================================

target_link_libraries(Modules PUBLIC
        ModuleCamera3D
        ModuleInput
        ModuleRenderer3D
        ModuleResourceManager
        ModuleScene
        ModuleWindow
)