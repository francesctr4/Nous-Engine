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
target_include_directories(Modules PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# =====================================================
# External dependencies
# =====================================================

find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(parson CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(glm REQUIRED)

target_link_libraries(Modules PUBLIC
        SDL3::SDL3
        Vulkan::Vulkan
        assimp::assimp
        imgui::imgui
        parson::parson
        Tracy::TracyClient
        glm::glm
)