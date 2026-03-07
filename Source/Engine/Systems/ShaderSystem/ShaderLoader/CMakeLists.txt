target_sources(ShaderSystem PRIVATE
        include/ShaderLoader.h
        include/ShaderLoaderTypes.h
        src/ShaderLoader.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()