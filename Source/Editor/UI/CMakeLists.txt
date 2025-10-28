# ==============================
# Nous Engine - EDITOR UI
# ==============================

add_subdirectory(ImGuiCustom)

set(EDITOR_UI_SOURCES
        Windows/AssetsBrowser/AssetsBrowser.cpp
        Windows/AssetsBrowser/AssetsBrowser.h
        Windows/GameViewport/GameViewport.cpp
        Windows/GameViewport/GameViewport.h
        Windows/JobQueueWindow/JobQueueWindow.cpp
        Windows/JobQueueWindow/JobQueueWindow.h
        Windows/MainMenuBar/MainMenuBar.cpp
        Windows/MainMenuBar/MainMenuBar.h
        Windows/MultithreadingWindow/MultithreadingWindow.cpp
        Windows/MultithreadingWindow/MultithreadingWindow.h
        Windows/ResourcesWindow/ResourcesWindow.cpp
        Windows/ResourcesWindow/ResourcesWindow.h
        Windows/SceneViewport/SceneViewport.cpp
        Windows/SceneViewport/SceneViewport.h
        Windows/HierarchyWindow/HierarchyWindow.cpp
        Windows/HierarchyWindow/HierarchyWindow.h
        Windows/InspectorWindow/InspectorWindow.cpp
        Windows/InspectorWindow/InspectorWindow.h
        Windows/ConsoleWindow/ConsoleWindow.cpp
        Windows/ConsoleWindow/ConsoleWindow.h

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