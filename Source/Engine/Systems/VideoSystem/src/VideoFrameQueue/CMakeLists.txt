set(CLASS_NAME "VideoFrameQueue")

# Fully private: the decoder's internal ring buffer. Consumers see VideoFrame and
# VideoHandle, never the queue.
target_sources(VideoSystem PRIVATE
        ${CLASS_NAME}.h
        ${CLASS_NAME}.cpp
)
