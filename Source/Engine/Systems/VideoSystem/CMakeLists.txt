set(CLASS_NAME "VideoSystem")

# ==============================================================================
# VideoSystem — public/private header layout (see CONVENTIONS.md).
#
#   include/VideoSystem/**  is the API, reached as <VideoSystem/VideoFrame.h>.
#   src/**                  is implementation, unreachable from outside.
#
# The split was decided by grepping consumers outside this target. Two headers
# turned out to be private, and both matter:
#
#   FFmpegBackend.h    `.claude/CLAUDE.md` calls VideoDecoderBackendFactory.cpp
#                      "the only TU that names FFmpegBackend -- the compile
#                      firewall keeping libav* out of the module layer". That was
#                      a sentence in a document; it is now a build rule, exactly
#                      as it is for MiniaudioBackend in AudioSystem.
#   VideoFrameQueue.h  the decoder's internal ring. ModuleVideo and CVideoPlayer
#                      only ever see VideoFrame/VideoHandle, so the queue is
#                      genuinely detail. Its test opts in via VideoSystem_private.
# ==============================================================================

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# EngineCore (Globals.h / EngineExport.h / UpdateStatus.h) is named by this
# target's PUBLIC headers, so it rides on the _headers handle: consumers of
# those headers need the dir too. It is header-only, so this adds no linkage.
target_link_libraries(${CLASS_NAME}_headers INTERFACE EngineCore_headers)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(${CLASS_NAME} STATIC)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/VideoHandle.h
        include/${CLASS_NAME}/VideoFrame.h
        include/${CLASS_NAME}/iVideoBroker.h
        include/${CLASS_NAME}/VideoDecoder/IVideoDecoderBackend.h
        include/${CLASS_NAME}/VideoDecoder/VideoDecoderBackendFactory.h
        include/${CLASS_NAME}/VideoProbe/VideoProbe.h
        include/${CLASS_NAME}/AudioExtract/AudioExtract.h
)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
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

target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
)

# ResourceManager closes a cycle -- same shape as AudioSystem's.
target_link_libraries(${CLASS_NAME} PRIVATE
        Logger
        MemoryManager
        ResourceManager
)

add_subdirectory(src/VideoDecoder)
add_subdirectory(src/VideoFrameQueue)
add_subdirectory(src/VideoProbe)
add_subdirectory(src/AudioExtract)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
