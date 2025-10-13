# ==============================
# Nous Engine - Editor Module
# ==============================

set(EDITOR_SOURCES
        ImGuiConfig/ImGuiCustom.cpp
        ImGuiConfig/ImGuiCustom.h

        Windows/AssetsBrowser.cpp
        Windows/AssetsBrowser.h
        Windows/GameViewport.cpp
        Windows/GameViewport.h
        Windows/JobQueueWindow.cpp
        Windows/JobQueueWindow.h
        Windows/MainMenuBar.cpp
        Windows/MainMenuBar.h
        Windows/MultithreadingWindow.cpp
        Windows/MultithreadingWindow.h
        Windows/ResourcesWindow.cpp
        Windows/ResourcesWindow.h
        Windows/SceneViewport.cpp
        Windows/SceneViewport.h
        Windows/HierarchyWindow.cpp
        Windows/HierarchyWindow.h
        Windows/InspectorWindow.cpp
        Windows/InspectorWindow.h
        Windows/ConsoleWindow.cpp
        Windows/ConsoleWindow.h

        IEditorWindow.inl
)

add_library(Editor STATIC ${EDITOR_SOURCES})

# Include the root source dir for headers like "Core/Application.h"
target_include_directories(Editor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Find dependencies
find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(parson CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(glm REQUIRED)

# Link editor dependencies
target_link_libraries(Editor PUBLIC
        SDL3::SDL3
        Vulkan::Vulkan
        assimp::assimp
        imgui::imgui
        parson::parson
        Tracy::TracyClient
        glm::glm
)

set_target_properties(Editor PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)