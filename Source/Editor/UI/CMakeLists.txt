# ==============================
# Nous Engine - EDITOR UI
# ==============================

# ----------------------------------------
# ImGuiColorTextEdit (two-file download → ThirdParty/)
# ----------------------------------------
set(IMGUI_COLOR_TEXT_EDIT_DIR "${CMAKE_SOURCE_DIR}/ThirdParty/ImGuiColorTextEdit")
set(IMGUI_COLOR_TEXT_EDIT_BASE_URL "https://raw.githubusercontent.com/BalazsJako/ImGuiColorTextEdit/master")

file(MAKE_DIRECTORY ${IMGUI_COLOR_TEXT_EDIT_DIR})

foreach(FILE TextEditor.h TextEditor.cpp)
    if(NOT EXISTS "${IMGUI_COLOR_TEXT_EDIT_DIR}/${FILE}")
        message(STATUS "Downloading ImGuiColorTextEdit/${FILE}...")
        file(DOWNLOAD
            "${IMGUI_COLOR_TEXT_EDIT_BASE_URL}/${FILE}"
            "${IMGUI_COLOR_TEXT_EDIT_DIR}/${FILE}"
            STATUS DOWNLOAD_STATUS
        )
        list(GET DOWNLOAD_STATUS 0 DOWNLOAD_CODE)
        if(NOT DOWNLOAD_CODE EQUAL 0)
            message(FATAL_ERROR "Failed to download ImGuiColorTextEdit/${FILE}")
        endif()
    endif()
endforeach()

find_package(imgui CONFIG REQUIRED)
find_package(imguizmo CONFIG REQUIRED)

add_library(ImGuiColorTextEdit STATIC
    ${IMGUI_COLOR_TEXT_EDIT_DIR}/TextEditor.h
    ${IMGUI_COLOR_TEXT_EDIT_DIR}/TextEditor.cpp
)
target_include_directories(ImGuiColorTextEdit SYSTEM PUBLIC
    ${IMGUI_COLOR_TEXT_EDIT_DIR}
)
target_link_libraries(ImGuiColorTextEdit PUBLIC imgui::imgui)
set_target_properties(ImGuiColorTextEdit PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
    FOLDER "ThirdParty"
)

# ----------------------------------------
# EditorUI target
# ----------------------------------------
add_library(EditorUI STATIC
        IEditorWindow.h
        IEditorWindow.cpp
)

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
target_include_directories(EditorUI SYSTEM PUBLIC
        ${IMGUI_COLOR_TEXT_EDIT_DIR}
)

target_compile_definitions(EditorUI PRIVATE NOUS_EDITOR_EXPORTS)

target_link_libraries(EditorUI PUBLIC
        ModuleEditor
        imgui::imgui
        imguizmo::imguizmo
        ImGuiCustom
        ImGuiColorTextEdit
        # AudioGraphEditor + AudioMixerWindow name AudioSystem's public types.
        # Headers only -- no objects, no transitive deps. Before AudioSystem was
        # converted, this dependency existed just the same but was invisible,
        # satisfied by the global Source/ include root.
        AudioSystem_headers
        # The foundation targets, converted 2026-08-27. Same story: the editor
        # windows have always included Logger/MemoryManager/FileSystem/the job
        # system; the dependency was satisfied invisibly by the global Source/
        # include root and is now declared.
        FileSystem_headers
        Logger_headers
        MemoryManager_headers
        NOUS_Multithreading_headers
)

# Converted 2026-08-27, and PRIVATE rather than PUBLIC because these are named
# only from .cpp: SceneViewport uses Camera, InspectorCMaterial the shader
# reflection types, ComponentInspectorRegistry the video audio-extract helper.
target_link_libraries(EditorUI PRIVATE
        CameraSystem_headers
        VideoSystem_headers
        PrefabManager_headers   # HierarchyWindow.cpp spawns prefabs
        # SceneViewport/GameViewport hold IEditorRenderBridge* and call the
        # frontend; both from .cpp only.
        Renderer_headers
        RendererFrontend_headers
)

# ShaderSystem is PUBLIC rather than PRIVATE: InspectorWindow.h is a public
# EditorUI header and reaches ShaderTypes.h through ResourceShader.h, so
# consumers of that header need the dir too.
target_link_libraries(EditorUI PUBLIC
        ShaderSystem_headers
        # HierarchyWindow.h and InspectorWindow.h name GameObject, Scene and the
        # component types in their public headers. InspectorWindow.h also holds
        # ResourceShader/ResourceTexture.
        ECS_headers
        ResourceManager_headers
)