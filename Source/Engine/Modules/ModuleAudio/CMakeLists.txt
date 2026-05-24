set(CLASS_NAME "ModuleAudio")

add_library(${CLASS_NAME} STATIC
        include/${CLASS_NAME}.h
        src/${CLASS_NAME}.cpp
)

target_include_directories(${CLASS_NAME} PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

# Include miniaudio
find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h")
target_include_directories(${CLASS_NAME} PRIVATE ${MINIAUDIO_INCLUDE_DIRS})

# Include stb (for stb_vorbis.c — enables .ogg decoding in miniaudio)
find_path(STB_INCLUDE_DIRS "stb_vorbis.c")
target_include_directories(${CLASS_NAME} PRIVATE ${STB_INCLUDE_DIRS})