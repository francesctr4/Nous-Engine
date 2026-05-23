target_sources(Utils PRIVATE
        Random.h
        Random.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()