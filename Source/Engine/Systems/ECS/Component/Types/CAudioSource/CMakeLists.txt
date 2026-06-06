set(CLASS_NAME "CAudioSource")

target_sources(ECS PRIVATE
        include/${CLASS_NAME}.h
        include/AudioVoice.h
        src/${CLASS_NAME}.cpp
        src/AudioVoice.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
