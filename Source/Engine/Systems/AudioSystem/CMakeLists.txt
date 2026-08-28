set(CLASS_NAME "AudioSystem")

# ==============================================================================
# AudioSystem — first target converted to the public/private header layout.
#
#   include/AudioSystem/**  is the API. Reachable from anywhere that links this
#                           target, as <AudioSystem/...>.
#   src/**                  is implementation. Reachable ONLY from inside this
#                           target, because src/ is a PRIVATE include dir.
#
# That second line is what the layout buys. `.claude/CLAUDE.md` states that
# AudioBackendFactory.cpp is "the only TU that names MiniaudioBackend — the
# compile firewall keeping miniaudio out of the module layer". Before this, that
# was a sentence in a document. Now the miniaudio headers live under src/ and
# simply do not resolve from outside, so the build system enforces it.
# ==============================================================================

# Two include-dir-only handles. They carry no objects and no link dependencies,
# which is what lets a header-only unit test compile against this target's code
# WITHOUT dragging in Logger/MemoryManager/ResourceManager. Linking the real
# AudioSystem target for a pure-policy test would pull all three.
#
#   AudioSystem_headers   the public API dir. AudioSystem links it PUBLIC, so
#                         ordinary consumers get it transitively and never name it.
#   AudioSystem_private   the implementation dir. Only tests of deliberately
#                         dependency-free policy code (GainDsp, AudioBusMath)
#                         should ever link this — it is the documented opt-in,
#                         instead of a test reaching in with ../../ paths.
add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# EngineCore (Globals.h / EngineExport.h / UpdateStatus.h) is named by this
# target's PUBLIC headers, so it rides on the _headers handle: consumers of
# those headers need the dir too. It is header-only, so this adds no linkage.
target_link_libraries(${CLASS_NAME}_headers INTERFACE EngineCore_headers)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(${CLASS_NAME} STATIC)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/SoundHandle.h
        include/${CLASS_NAME}/EffectChainHandle.h
        include/${CLASS_NAME}/AudioTypes.h
        include/${CLASS_NAME}/iAudioBroker.h
        include/${CLASS_NAME}/AudioEngine/IAudioEngineBackend.h
        include/${CLASS_NAME}/AudioEngine/AudioBackendFactory.h
        include/${CLASS_NAME}/AudioGraph/AudioEffectTypes.h
        include/${CLASS_NAME}/AudioGraph/AudioEffectRegistry.h
        include/${CLASS_NAME}/AudioProbe/AudioProbe.h
)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE
        NOUS_ENGINE_EXPORTS
)

# ResourceManager closes a cycle: ResourceAudio/ResourceAudioGraph live there,
# while the probe + effect registry live here and the importers call back into
# them. CMake permits cycles between STATIC libraries.
target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
)

target_link_libraries(${CLASS_NAME} PRIVATE
        Logger
        MemoryManager
        ResourceManager
)

add_subdirectory(src/AudioEngine)
add_subdirectory(src/AudioGraph)
add_subdirectory(src/AudioMixer)
add_subdirectory(src/AudioProbe)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
