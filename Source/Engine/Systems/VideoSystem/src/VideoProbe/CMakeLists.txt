set(CLASS_NAME "VideoProbe")

# Private half only: VideoProbe.h is API (ImporterVideo calls it) and lives at
# include/VideoSystem/VideoProbe/, listed in the target root's CMakeLists.
target_sources(VideoSystem PRIVATE
        ${CLASS_NAME}.cpp
)
