set(CLASS_NAME "AudioBusMath")

# Backend-agnostic bus policy (mute/solo math). The miniaudio realization that
# consumes it lives in AudioEngine/Backends/miniaudio/MiniaudioBusGraph.
target_sources(AudioSystem PRIVATE
        include/${CLASS_NAME}.h
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
