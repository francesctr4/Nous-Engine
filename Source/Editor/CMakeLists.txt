# ==============================
# Nous Engine - Editor Module
# ==============================

# ----------------------------------------
# Editor Library Target
# ----------------------------------------

set(EDITOR_NAME "Nous-Editor")

add_subdirectory(ModuleEditor)
add_subdirectory(GameExporter)
add_subdirectory(UI)

add_library(${EDITOR_NAME} SHARED EditorExport.h EditorContext.h)
add_library(NousEngine::Editor ALIAS ${EDITOR_NAME})

target_link_libraries(${EDITOR_NAME} PUBLIC
        NousEngine::Engine
        EditorUI
        ModuleEditor
)

if (WIN32)
  target_link_options(${EDITOR_NAME} PRIVATE
    "LINKER:/WHOLEARCHIVE:$<TARGET_FILE:ModuleEditor>"
    "LINKER:/WHOLEARCHIVE:$<TARGET_FILE:EditorUI>"
  )
elseif (UNIX)
  # Mirror Windows /WHOLEARCHIVE: bake all static-lib symbols into the .so so
  # consumers never hit circular-dependency ordering failures with the archives.
  target_link_options(${EDITOR_NAME} PRIVATE
    "-Wl,--whole-archive,$<TARGET_FILE:ModuleEditor>,$<TARGET_FILE:EditorUI>,--no-whole-archive"
  )
endif()

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

add_executable(EditorApp
        MainEditor.cpp
)

target_link_libraries(EditorApp PRIVATE
        NousEngine::Editor
)

set_target_properties(EditorApp PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(EditorApp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)
