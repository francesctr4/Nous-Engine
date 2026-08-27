set(CLASS_NAME "CVideoPlayer")

# The one unit in ECS with a genuinely private header: VideoPlayhead.h is pure
# playhead-selection policy, named only by CVideoPlayer.cpp and t_VideoPlayhead
# (which reaches it through ECS_private). CVideoPlayer.h itself is API.
target_sources(ECS PRIVATE
        ${CLASS_NAME}.cpp
        VideoPlayhead.h
        VideoPlayhead.cpp
)
