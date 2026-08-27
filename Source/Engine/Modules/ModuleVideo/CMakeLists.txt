set(CLASS_NAME "ModuleVideo")

add_library(${CLASS_NAME} STATIC
        include/${CLASS_NAME}.h
        src/${CLASS_NAME}.cpp
)

target_include_directories(${CLASS_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(${CLASS_NAME} PUBLIC
        ModuleBase
        # ModuleVideo.h names VideoFrame, VideoHandle and IVideoBroker in its
        # PUBLIC header, so the API dir has to propagate (Application.cpp includes
        # it). The VideoSystem objects stay PRIVATE below -- headers only.
        VideoSystem_headers
)

target_link_libraries(${CLASS_NAME} PRIVATE
        EventSystem
        Logger
        MemoryManager
        VideoSystem
)
