target_sources(AudioSystem PRIVATE
        IAudioEngineBackend.h
        AudioBackendFactory.h
        AudioBackendFactory.cpp
)

add_subdirectory(Backends)