# ==============================
# Nous Engine - EDITOR UI
# ==============================

add_subdirectory(ImGuiCustom)

set(EDITOR_UI_SOURCES
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

add_library(EditorUI STATIC ${EDITOR_UI_SOURCES})

set_target_properties(EditorUI PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(EditorUI PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(EditorUI PRIVATE NOUS_EDITOR_EXPORTS)

find_package(imgui CONFIG REQUIRED)

target_link_libraries(EditorUI PUBLIC
        imgui::imgui
        ImGuiCustom
)