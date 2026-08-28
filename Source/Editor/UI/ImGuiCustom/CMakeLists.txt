set(CLASS_NAME "ImGuiCustom")

# ==============================================================================
# ImGuiCustom — converted to the public/private header layout (see CONVENTIONS.md).
#
#   include/ImGuiCustom/**  is the API, reached as <ImGuiCustom/ImGuiCustom.h>.
#   src/**                  is implementation, reachable only from inside this target.
#
# One header, and it is public: EditorUI's windows and ModuleEditor.cpp both draw
# with it. No private headers today.
# ==============================================================================

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(${CLASS_NAME} STATIC
        src/${CLASS_NAME}.cpp
)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/${CLASS_NAME}.h
)

target_include_directories(${CLASS_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

find_package(imgui CONFIG REQUIRED)

# imgui is PUBLIC: ImGuiCustom.h's signatures are ImGui types.
target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
        imgui::imgui
)
