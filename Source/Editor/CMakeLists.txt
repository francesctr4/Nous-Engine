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
elseif (APPLE)
  # macOS ld64 equivalent: -force_load per archive (--whole-archive is GNU ld only)
  target_link_options(${EDITOR_NAME} PRIVATE
    "-Wl,-force_load,$<TARGET_FILE:ModuleEditor>"
    "-Wl,-force_load,$<TARGET_FILE:EditorUI>"
  )
elseif (UNIX)
  # Linux: bake all static-lib symbols into the .so so consumers never hit
  # circular-dependency ordering failures with the raw archives.
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
        SanitizerDefaults.cpp
)

# ----------------------------------------
# Sanitizer runtime defaults
# ----------------------------------------
# Points TSan at Tools/Sanitizers/tsan.supp via the __tsan_default_options() hook
# rather than TSAN_OPTIONS, so the suppressions apply however the binary is
# launched — and so CLion keeps ownership of TSAN_OPTIONS for its log_path (which
# is what drives the clickable "Sanitizers" tab).
if(NOUS_SANITIZE MATCHES "thread")
        target_compile_definitions(EditorApp PRIVATE
                NOUS_TSAN_SUPPRESSIONS="${CMAKE_SOURCE_DIR}/Tools/Sanitizers/tsan.supp")
endif()

if(NOUS_SANITIZE MATCHES "undefined")
        target_compile_definitions(EditorApp PRIVATE NOUS_UBSAN_ENABLED=1)
endif()

# ----------------------------------------
# Application icon (per-platform)
# ----------------------------------------
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Branding/EditorApp.ico")
        # The .rc compiles into the exe; rc resolves "EditorApp.ico" relative to itself.
        target_sources(EditorApp PRIVATE Branding/EditorApp.rc)
# elseif(APPLE)
#         set(EDITOR_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/Branding/EditorApp.icns")
#         set_source_files_properties(${EDITOR_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
#         target_sources(EditorApp PRIVATE ${EDITOR_ICNS})
#         set_target_properties(EditorApp PROPERTIES
#                 MACOSX_BUNDLE TRUE
#                 MACOSX_BUNDLE_ICON_FILE "EditorApp.icns")
# elseif(UNIX)
#         install(FILES Branding/EditorApp.desktop DESTINATION share/applications)
#         install(FILES Branding/EditorApp-256.png
#                 DESTINATION share/icons/hicolor/256x256/apps RENAME nous-editor.png)
endif()

target_link_libraries(EditorApp PRIVATE
        NousEngine::Editor
)

set_target_properties(EditorApp PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(EditorApp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
)
