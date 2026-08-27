# Private half only: IVideoDecoderBackend.h and VideoDecoderBackendFactory.h are
# API (ModuleVideo names both) and live under include/VideoSystem/VideoDecoder/,
# listed in the target root's CMakeLists.
target_sources(VideoSystem PRIVATE
        VideoDecoderBackendFactory.cpp
)

add_subdirectory(Backends)
