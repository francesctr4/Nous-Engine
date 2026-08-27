# ==============================
# Nous Engine - ECS
#
# Public/private header layout (see CONVENTIONS.md).
#
#   include/ECS/**  is the API, reached as <ECS/GameObject.h>.
#   src/**          is implementation, unreachable from outside.
#
# ECS is almost entirely API, and that is the honest result rather than a
# shortcut: a component library's components ARE its interface. The empirical
# grep (who includes what from outside the target) put 19 of 20 headers in
# include/. Two that looked private on first count are not, and the reason is
# worth keeping:
#
#   ComponentList.h  included by the public ComponentTypes.h
#   AudioVoice.h     included by the public CAudioSource.h (it holds two)
#
# A header reached through a public header is itself public, however few direct
# consumers it has. Counting includers alone would have moved both to src/ and
# broken every consumer.
#
#   VideoPlayhead.h  is the one genuine private: pure playhead-selection policy,
#                    named only by CVideoPlayer.cpp and its own test. Same shape
#                    as AudioSystem's GainDsp -- and t_VideoPlayhead is now a
#                    real unit test, reaching it through ECS_private.
#
# include/ mirrors a folder per component under Component/Types/, matching src/
# and test/. Adding a component is a recurring workflow, so a component's files
# stay one identically-named folder in each of the three trees -- the checklist
# is "the three CFoo folders". Same reasoning as ResourceManager's Types/.
#
# ${CMAKE_SOURCE_DIR}/Source stays PUBLIC until the last target is converted.
# ==============================

set(CLASS_NAME "ECS")

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Third handle, unique to ECS so far: FakeComponentServices.h is a recording test
# double shared by seven of this target's own tests. It is not API (no shipping
# code may see it) and not implementation (it is not in src/), so neither of the
# other two handles fits. Tests that need it link this and include
# <FakeComponentServices.h>. Nothing outside test/ may link it.
add_library(${CLASS_NAME}_test_headers INTERFACE)
target_include_directories(${CLASS_NAME}_test_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/test)

add_library(${CLASS_NAME} STATIC)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/ComponentServices.h
        include/${CLASS_NAME}/ECSInternalComponents.h
        include/${CLASS_NAME}/GameObject.h
        include/${CLASS_NAME}/Scene/Scene.h
        include/${CLASS_NAME}/Scene/iSceneHost.h
        include/${CLASS_NAME}/Component/Component.h
        include/${CLASS_NAME}/Component/ComponentList.h
        include/${CLASS_NAME}/Component/Types/ComponentTypes.h
        include/${CLASS_NAME}/Component/Types/CAudioSource/AudioVoice.h
        include/${CLASS_NAME}/Component/Types/CAudioListener/CAudioListener.h
        include/${CLASS_NAME}/Component/Types/CAudioSource/CAudioSource.h
        include/${CLASS_NAME}/Component/Types/CCamera/CCamera.h
        include/${CLASS_NAME}/Component/Types/CLight/CLight.h
        include/${CLASS_NAME}/Component/Types/CMaterial/CMaterial.h
        include/${CLASS_NAME}/Component/Types/CMesh/CMesh.h
        include/${CLASS_NAME}/Component/Types/CPrefab/CPrefab.h
        include/${CLASS_NAME}/Component/Types/CScript/CScript.h
        include/${CLASS_NAME}/Component/Types/CTransform/CTransform.h
        include/${CLASS_NAME}/Component/Types/CVideoPlayer/CVideoPlayer.h
)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME}
        PUBLIC  ${CMAKE_SOURCE_DIR}/Source          # temporary, see note above
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
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

target_link_libraries(${CLASS_NAME} PRIVATE
        CameraSystem
        Scripting_headers   # CScript.cpp names IScriptRegistry
        FileSystem
        Logger
        PrefabManager
        ResourceManager
)

add_subdirectory(src/Scene)
add_subdirectory(src/GameObject)
add_subdirectory(src/Component)

# FakeComponentServices.h is test-only and deliberately NOT in target_sources(ECS).
if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
