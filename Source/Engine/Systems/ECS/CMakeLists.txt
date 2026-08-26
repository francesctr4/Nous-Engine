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
)

target_link_libraries(ECS PRIVATE
        Camera
        FileSystem
        Logger
        PrefabManager
        ResourceManager
)