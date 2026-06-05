set(CLASS_NAME "Video")

target_sources(ResourceManager PRIVATE
        include/Resource${CLASS_NAME}.h
        src/Resource${CLASS_NAME}.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
