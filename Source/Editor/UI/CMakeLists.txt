# ==============================
# Nous Engine - EDITOR UI
# ==============================

add_library(EditorUI STATIC IEditorWindow.inl)

add_subdirectory(ImGuiCustom)
add_subdirectory(Windows)

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