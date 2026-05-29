target_sources(ResourceManager PRIVATE
        include/TypeRegistry.h
        src/TypeRegistry.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
