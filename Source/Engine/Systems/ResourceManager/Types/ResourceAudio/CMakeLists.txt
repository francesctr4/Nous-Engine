set(CLASS_NAME "Audio")

target_sources(ResourceManager PRIVATE
        include/Importer${CLASS_NAME}.h
        include/Resource${CLASS_NAME}.h
        src/Importer${CLASS_NAME}.cpp
        src/Resource${CLASS_NAME}.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()