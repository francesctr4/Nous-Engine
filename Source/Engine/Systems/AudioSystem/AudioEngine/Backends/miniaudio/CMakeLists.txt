target_sources(AudioSystem PRIVATE
        miniaudioBackend.cpp
        miniaudioBackend.h
)

# Include miniaudio
find_path(MINIAUDIO_INCLUDE_DIRS "miniaudio.h")
target_include_directories(AudioSystem PRIVATE ${MINIAUDIO_INCLUDE_DIRS})

# Include stb (for stb_vorbis.c — enables .ogg decoding in miniaudio)
find_path(STB_INCLUDE_DIRS "stb_vorbis.c")
target_include_directories(AudioSystem PRIVATE ${STB_INCLUDE_DIRS})