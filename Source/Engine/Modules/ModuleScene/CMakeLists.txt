set(CLASS_NAME "ModuleScene")

# ==============================================================================
# ModuleScene -- converted to the public/private header layout (see CONVENTIONS.md).
#
#   include/ModuleScene/**  is the API, reached as <ModuleScene/ModuleScene.h>.
#   src/**             is implementation, reachable only from inside this target.
#
# Both headers are public. SceneRenderData.h has ONE includer outside this
# target (ModuleRenderer3D.cpp), which does not decide it -- ModuleScene.h,
# the public header, includes it too, so it is API either way.
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
        include/${CLASS_NAME}/SceneRenderData.h
)

target_include_directories(${CLASS_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
        glm::glm
        PrefabManager
        ModuleBase
        ECS
        EnTT::EnTT
        EventSystem_headers   # <Module>.h implements IEventListener
        Scripting_headers   # ModuleScene.h implements IScriptSceneHost
)

target_link_libraries(${CLASS_NAME} PRIVATE
        CameraSystem
        EventSystem
        FileSystem
        Logger
        MemoryManager
        ModuleInput
        ModuleResourceManager
        NOUS_Multithreading
        ResourceManager
        SDL3::SDL3
        Scripting
        TimeManager
)

# ModuleScene.cpp includes <tracy/Tracy.hpp> under _PROFILING. The vcpkg tracy
# port installs its headers at include/tracy/tracy/Tracy.hpp, so that spelling
# resolves ONLY through the include dir Tracy::TracyClient carries -- it is not
# reachable from the vcpkg_installed/include root.
if (ENABLE_PROFILING)
    find_package(Tracy CONFIG REQUIRED)
    target_link_libraries(${CLASS_NAME} PRIVATE Tracy::TracyClient)
endif()

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
