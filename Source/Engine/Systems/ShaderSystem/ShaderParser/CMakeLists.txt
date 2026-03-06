target_sources(ShaderSystem PRIVATE
        include/ShaderParser.h
        src/ShaderParser.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()