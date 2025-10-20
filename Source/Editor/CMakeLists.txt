# ==============================
# Nous Engine - Editor Module
# ==============================

# ----------------------------------------
# Editor Library Target
# ----------------------------------------

set(EDITOR_NAME "Nous-Editor")

set(EDITOR_SOURCES
        Core/ModuleEditor.cpp
        Core/ModuleEditor.h

        UI/ImGuiConfig/ImGuiCustom.cpp
        UI/ImGuiConfig/ImGuiCustom.h

        UI/Windows/AssetsBrowser.cpp
        UI/Windows/AssetsBrowser.h
        UI/Windows/GameViewport.cpp
        UI/Windows/GameViewport.h
        UI/Windows/JobQueueWindow.cpp
        UI/Windows/JobQueueWindow.h
        UI/Windows/MainMenuBar.cpp
        UI/Windows/MainMenuBar.h
        UI/Windows/MultithreadingWindow.cpp
        UI/Windows/MultithreadingWindow.h
        UI/Windows/ResourcesWindow.cpp
        UI/Windows/ResourcesWindow.h
        UI/Windows/SceneViewport.cpp
        UI/Windows/SceneViewport.h
        UI/Windows/HierarchyWindow.cpp
        UI/Windows/HierarchyWindow.h
        UI/Windows/InspectorWindow.cpp
        UI/Windows/InspectorWindow.h
        UI/Windows/ConsoleWindow.cpp
        UI/Windows/ConsoleWindow.h
        UI/IEditorWindow.inl

        EditorExport.h
)

add_library(${EDITOR_NAME} SHARED ${EDITOR_SOURCES})
add_library(NousEngine::Editor ALIAS ${EDITOR_NAME})

target_compile_definitions(${EDITOR_NAME} PRIVATE NOUS_EDITOR_EXPORTS)

# Includes
target_include_directories(${EDITOR_NAME}
        PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# Dependencies
find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)

target_link_libraries(${EDITOR_NAME}
        PUBLIC
        NousEngine::Engine
        SDL3::SDL3
        Vulkan::Vulkan
        imgui::imgui
        Tracy::TracyClient
)

add_dependencies(${EDITOR_NAME} ${PROJECT_NAME})

set_target_properties(${EDITOR_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        FOLDER "Editor"
)

# ----------------------------------------
# Editor Executable Target
# ----------------------------------------

add_executable(EditorApp MainEditor.cpp)

target_include_directories(EditorApp
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${STB_INCLUDE_PATH}
)

target_link_libraries(EditorApp
        PRIVATE
        NousEngine::Engine
        NousEngine::Editor
)

set_target_properties(EditorApp PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        FOLDER "Applications"
)

add_dependencies(EditorApp ${EDITOR_NAME})