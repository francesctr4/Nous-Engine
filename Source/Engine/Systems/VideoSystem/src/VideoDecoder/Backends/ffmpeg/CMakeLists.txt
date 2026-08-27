# Fully private: the header stays here, which is what keeps libav* out of every
# other target. Only VideoDecoderBackendFactory.cpp may name FFmpegBackend.
target_sources(VideoSystem PRIVATE
        FFmpegBackend.cpp
        FFmpegBackend.h
)
