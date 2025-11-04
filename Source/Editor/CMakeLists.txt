# ==============================
# Nous Engine - Editor Module
# ==============================

# ----------------------------------------
# Editor Library Target
# ----------------------------------------

set(EDITOR_NAME "Nous-Editor")

add_subdirectory(ModuleEditor)
add_subdirectory(UI)

add_library(${EDITOR_NAME} SHARED EditorExport.h)
add_library(NousEngine::Editor ALIAS ${EDITOR_NAME})

target_link_libraries(${EDITOR_NAME} PUBLIC
        NousEngine::Engine
        ModuleEditor
        EditorUI
)

target_link_options(${EDITOR_NAME} PRIVATE
        "/WHOLEARCHIVE:$<TARGET_FILE:ModuleEditor>"
        "/WHOLEARCHIVE:$<TARGET_FILE:EditorUI>"
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
)

target_link_libraries(EditorApp PRIVATE
        NousEngine::Editor
)

set_target_properties(EditorApp PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        FOLDER "Applications"
)

add_dependencies(EditorApp ${EDITOR_NAME})