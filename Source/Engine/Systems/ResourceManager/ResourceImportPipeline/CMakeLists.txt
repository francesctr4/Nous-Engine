set(CLASS_NAME "ResourceImportPipeline")

target_sources(ResourceManager PRIVATE
        include/${CLASS_NAME}.h
        src/${CLASS_NAME}.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()