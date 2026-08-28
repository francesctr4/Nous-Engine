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
# ==============================================================================
# EditorUI -- converted to the public/private header layout (see CONVENTIONS.md).
#
#   include/EditorUI/**  is the API, reached as <EditorUI/SceneViewport.h>.
#   src/**               is implementation, reachable only from inside this target.
#
# The Windows/ tree is NOT a set of targets -- every leaf is a target_sources()
# into this one target -- so all 16 window headers share the single EditorUI
# prefix rather than getting one each.
#
# THREE headers are genuinely private, the first real private set in the editor:
# ComponentInspectorRegistry.h plus the two per-component drawers it owns
# (InspectorCMaterial.h, InspectorCScript.h). Nothing outside this target names
# them and no public header reaches them, so the registry is now the only door to
# the per-component inspectors -- which was always the design, and is now enforced.
#
# AudioGraphLinearize.h is the counter-example and stays PUBLIC: it too has no
# external includer, but AudioGraphEditor.h includes it, and a header reached
# THROUGH a public header is public whatever the includer count says.
#
# ${CMAKE_SOURCE_DIR}/Source stays PUBLIC until the last target is converted.
# ==============================================================================

add_library(EditorUI_headers INTERFACE)
target_include_directories(EditorUI_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# EngineCore (Globals.h / EngineExport.h / UpdateStatus.h) is named by this
# target's PUBLIC headers, so it rides on the _headers handle: consumers of
# those headers need the dir too. It is header-only, so this adds no linkage.
target_link_libraries(EditorUI_headers INTERFACE EngineCore_headers)

# An _headers handle must carry the header-level deps of the API it exposes.
# TextEditorWindow.h includes <TextEditor.h>, so anyone handed EditorUI's API dir
# alone -- ModuleEditor.cpp constructs every window -- needs this dir too. It used
# to arrive as the relative path "Editor/../../ThirdParty/...", which only ever
# resolved through the global Source/ root and would have broken when that goes.
target_include_directories(EditorUI_headers SYSTEM INTERFACE ${IMGUI_COLOR_TEXT_EDIT_DIR})

add_library(EditorUI_private INTERFACE)
target_include_directories(EditorUI_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(EditorUI STATIC
        src/IEditorWindow.cpp
)

target_sources(EditorUI PRIVATE
        include/EditorUI/IEditorWindow.h
        include/EditorUI/AssetsBrowser.h
        include/EditorUI/AudioGraphEditor.h
        include/EditorUI/AudioGraphLinearize.h
        include/EditorUI/AudioMixerWindow.h
        include/EditorUI/ConsoleWindow.h
        include/EditorUI/GameViewport.h
        include/EditorUI/HierarchyWindow.h
        include/EditorUI/InspectorWindow.h
        include/EditorUI/JobQueueWindow.h
        include/EditorUI/MainMenuBar.h
        include/EditorUI/MemoryWindow.h
        include/EditorUI/MultithreadingWindow.h
        include/EditorUI/ResourcesWindow.h
        include/EditorUI/SceneViewport.h
        include/EditorUI/TextEditorWindow.h
)

add_subdirectory(ImGuiCustom)
add_subdirectory(src/Windows)

set_target_properties(EditorUI PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(EditorUI
        PUBLIC  ${CMAKE_SOURCE_DIR}/Source          # temporary, see note above
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)
target_include_directories(EditorUI SYSTEM PUBLIC
        ${IMGUI_COLOR_TEXT_EDIT_DIR}
)

target_compile_definitions(EditorUI PRIVATE NOUS_EDITOR_EXPORTS)

target_link_libraries(EditorUI PUBLIC
        EditorUI_headers
        EditorCore
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
        # Converted 2026-08-27 alongside them. Utils is PUBLIC because
        # AssetsBrowser.h/HierarchyWindow.h/InspectorWindow.h reach
        # <Utils/Serialization/JsonObject.h> through Component.h.
        Utils_headers
        EventSystem_headers
        FileWatcher_headers
        Scripting_headers   # InspectorWindow.h includes IScript.inl
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
        # The editor windows reach the modules through EditorContext, from .cpp
        # only -- no public EditorUI header names a module, so all PRIVATE.
        ModuleBase_headers
        ModuleAudio_headers
        ModuleCamera3D_headers
        ModuleInput_headers
        ModuleResourceManager_headers
        ModuleScene_headers
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
# Tests moved from the leaves to a top-level test/ mirroring src/, so EditorUI's
# whole suite is visible in one place (see CONVENTIONS.md).
if (NOUS_BUILD_TESTS)
    add_subdirectory(test/Windows/AudioGraphEditor)
    add_subdirectory(test/Windows/InspectorWindow/ComponentInspectorRegistry)
endif()
