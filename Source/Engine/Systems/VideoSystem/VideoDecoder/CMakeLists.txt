target_sources(VideoSystem PRIVATE
        IVideoDecoderBackend.h
        VideoDecoderBackendFactory.h
        VideoDecoderBackendFactory.cpp
)

add_subdirectory(Backends)
