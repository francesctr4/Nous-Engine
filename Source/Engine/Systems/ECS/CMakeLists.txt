# ==============================
# Nous Engine - ECS
# ==============================

add_library(ECS STATIC)

target_sources(ECS PRIVATE
        ComponentServices.h
)

add_subdirectory(Scene)
add_subdirectory(GameObject)
add_subdirectory(Component)

# FakeComponentServices.h is test-only and deliberately NOT in target_sources(ECS).
if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()

set_target_properties(ECS PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(ECS PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(ECS PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(ECS PUBLIC
        glm::glm
        EnTT::EnTT
        Renderer          # RendererTypes.h in CLight's public header
        Utils             # JsonObject in every component's Serialize/Deserialize
        # CAudioSource.h / AudioVoice.h name SoundHandle, AudioBus and
        # IAudioBroker. Headers only: ECS links no AudioSystem objects.
        AudioSystem_headers
        # CVideoPlayer.h names VideoFrame and VideoHandle in its public header,
        # so this one is PUBLIC too. Headers only: ECS links no VideoSystem
        # objects either.
        VideoSystem_headers
        # GameObject.h -- a public ECS header -- includes <Logger/Asserts.h>, so
        # the API dir must propagate. The Logger objects stay PRIVATE below.
        Logger_headers
)

target_link_libraries(ECS PRIVATE
        CameraSystem
        FileSystem
        Logger
        PrefabManager
        ResourceManager
)