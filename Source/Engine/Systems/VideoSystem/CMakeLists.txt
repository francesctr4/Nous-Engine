set(CLASS_NAME "VideoSystem")

add_library(${CLASS_NAME} STATIC
        VideoHandle.h
        VideoFrame.h
)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(${CLASS_NAME} PRIVATE
        NOUS_ENGINE_EXPORTS
)

# FFmpeg (vcpkg provides the FFMPEG find module) — PRIVATE so libav* headers
# stay confined to the VideoSystem target (VideoProbe.cpp + FFmpegBackend.cpp).
find_package(FFMPEG REQUIRED)
target_include_directories(${CLASS_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})
target_link_directories(${CLASS_NAME} PRIVATE ${FFMPEG_LIBRARY_DIRS})
target_link_libraries(${CLASS_NAME} PRIVATE ${FFMPEG_LIBRARIES})

add_subdirectory(VideoDecoder)
add_subdirectory(VideoFrameQueue)
add_subdirectory(VideoProbe)
add_subdirectory(AudioExtract)
