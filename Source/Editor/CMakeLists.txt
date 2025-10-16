# ==============================
# Nous Engine - Editor Module
# ==============================

set(EDITOR_SOURCES
        ModuleEditor.cpp
        ModuleEditor.h

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

        EditorExport.h
)

add_library(Nous-Editor SHARED ${EDITOR_SOURCES})
add_library(NousEngine::Editor ALIAS Nous-Editor)

target_compile_definitions(Nous-Editor PRIVATE NOUS_EDITOR_EXPORTS)

# Include the root source dir for headers like "Core/Application.h"
target_include_directories(Nous-Editor PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)

# Find dependencies
find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(parson CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(glm REQUIRED)

# Link editor dependencies
target_link_libraries(Nous-Editor PUBLIC
        NousEngine::Engine
        SDL3::SDL3
        Vulkan::Vulkan
        assimp::assimp
        imgui::imgui
        parson::parson
        Tracy::TracyClient
        glm::glm
)

add_dependencies(Nous-Editor ${PROJECT_NAME})

set_target_properties(Nous-Editor PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

set_target_properties(Nous-Editor PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        FOLDER "Editor"
)