target_sources(Utils PRIVATE
        JsonFile.h
        JsonFile.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()