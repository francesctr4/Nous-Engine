# Implementation only. The public headers (IAudioEngineBackend.h,
# AudioBackendFactory.h) live under include/AudioSystem/AudioEngine/ and are
# listed by the target root.
target_sources(AudioSystem PRIVATE
        AudioBackendFactory.cpp
)

add_subdirectory(Backends)
