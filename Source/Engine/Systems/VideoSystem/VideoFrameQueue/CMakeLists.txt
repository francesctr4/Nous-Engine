set(CLASS_NAME "VideoFrameQueue")

target_sources(VideoSystem PRIVATE
        include/${CLASS_NAME}.h
        src/${CLASS_NAME}.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()