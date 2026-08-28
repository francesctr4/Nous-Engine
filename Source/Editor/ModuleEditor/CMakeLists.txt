set(CLASS_NAME "ModuleEditor")

# ==============================================================================
# ModuleEditor -- converted to the public/private header layout (see CONVENTIONS.md).
#
#   include/ModuleEditor/**  is the API, reached as <ModuleEditor/ModuleEditor.h>.
#   src/**                   is implementation, reachable only from inside this target.
#
# GameExporter.h lives here too, though its source sits in Editor/GameExporter/:
# that folder is a target_sources() leaf of THIS target, and a target owns exactly
# one include tree.
# ==============================================================================

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# EngineCore (Globals.h / EngineExport.h / UpdateStatus.h) is named by this
# target's PUBLIC headers, so it rides on the _headers handle: consumers of
# those headers need the dir too. It is header-only, so this adds no linkage.
target_link_libraries(${CLASS_NAME}_headers INTERFACE EngineCore_headers)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(${CLASS_NAME} STATIC
        src/${CLASS_NAME}.cpp
)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/${CLASS_NAME}.h
        include/${CLASS_NAME}/GameExporter.h
)

target_include_directories(${CLASS_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_EDITOR_EXPORTS)

find_package(glm CONFIG REQUIRED)
find_package(SDL3 CONFIG REQUIRED)

# GameExporter.cpp calls SDL_GetBasePath directly. No editor target had ever
# declared SDL3: it resolved because EditorUI drags it in transitively and
# Nous-Editor links both archives into one DLL. Linking ModuleEditor on its own
# (t_GameExporter) is what surfaced it.
target_link_libraries(${CLASS_NAME} PRIVATE SDL3::SDL3)

target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
        # ModuleEditor.h implements EditorContext.
        EditorCore
        glm::glm
        # ModuleEditor.cpp includes AudioGraphEditor.h, which names the effect
        # registry. Headers only.
        AudioSystem_headers
        # ModuleEditor.h includes NOUS_Vector.h, which names NOUS_STLAllocator.
        MemoryManager_headers
        # NOUS_Vector.h itself lives in Utils, and ModuleEditor.h implements
        # IEventListener -- both reached from the public header, so both PUBLIC.
        Utils_headers
        EventSystem_headers
        # ModuleEditor.h implements IEditorOverlay, so this one is PUBLIC.
        RendererFrontend_headers
        # ModuleEditor derives from Module, so ModuleBase is PUBLIC too.
        ModuleBase_headers
)

target_link_libraries(${CLASS_NAME} PRIVATE
        FileSystem_headers
        Logger_headers
        # ModuleEditor.cpp compiles RendererFrontend.h (shader loader types) and
        # the Multithreading/JobQueue windows (job system).
        NOUS_Multithreading_headers
        ShaderSystem_headers
        ECS_headers
        ResourceManager_headers
        FileWatcher_headers
        # ModuleEditor.cpp drives these three; its public header names none of them.
        ModuleInput_headers
        ModuleRenderer3D_headers
        ModuleWindow_headers
        # ModuleEditor.cpp constructs every editor window and draws with
        # ImGuiCustom; ModuleEditor.h names neither, so both stay PRIVATE.
        EditorUI_headers
        ImGuiCustom_headers
        Scripting_headers
)

# ModuleEditor.cpp reaches the backend through IEditorRenderBridge; its own public
# header does not name it, so PRIVATE is the honest scope.
target_link_libraries(${CLASS_NAME} PRIVATE
        Renderer_headers
)
