set(CLASS_NAME "CVideoPlayer")

target_sources(ECS PRIVATE
        VideoPlayhead.h
        VideoPlayhead.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
