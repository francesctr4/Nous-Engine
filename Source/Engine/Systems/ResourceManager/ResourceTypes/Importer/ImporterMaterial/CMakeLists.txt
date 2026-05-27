set(CLASS_NAME "ImporterMaterial")

target_sources(ResourceManager PRIVATE
        include/${CLASS_NAME}.h
        src/${CLASS_NAME}.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()