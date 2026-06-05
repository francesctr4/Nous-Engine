set(CLASS_NAME "CVideoPlayer")

target_sources(ECS PRIVATE
        VideoPlayhead.h
        VideoPlayhead.cpp
        include/${CLASS_NAME}.h
        src/${CLASS_NAME}.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
