set(CLASS_NAME "AudioGraph")

# Header-only, backend-agnostic effect recipe + schema registry. The miniaudio
# realization that consumes these (effect chain + node factory) arrives in Layer 2
# under AudioEngine/Backends/miniaudio/.
target_sources(AudioSystem PRIVATE
        AudioEffectTypes.h
        AudioEffectRegistry.h
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
