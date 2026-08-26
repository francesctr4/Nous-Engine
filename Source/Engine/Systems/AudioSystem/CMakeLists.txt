set(CLASS_NAME "AudioSystem")

add_library(${CLASS_NAME} STATIC
        SoundHandle.h
        EffectChainHandle.h
        iAudioBroker.h
)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(${CLASS_NAME} PRIVATE
        NOUS_ENGINE_EXPORTS
)

# ResourceManager closes a cycle: ResourceAudio/ResourceAudioGraph live there,
# while the probe + effect registry live here and the importers call back into
# them. CMake permits cycles between STATIC libraries.
target_link_libraries(${CLASS_NAME} PRIVATE
        Logger
        MemoryManager
        ResourceManager
)

add_subdirectory(AudioEngine)
add_subdirectory(AudioGraph)
add_subdirectory(AudioMixer)
add_subdirectory(AudioProbe)