target_sources(ShaderSystem PRIVATE
        include/ShaderCompiler.h
        include/ShaderCompilerTypes.h
        src/ShaderCompiler.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()