target_sources(ShaderSystem PRIVATE
        include/ShaderReflection.h
        include/ShaderReflectionTypes.h
        include/ShaderReflectionSerializer.h
        src/ShaderReflection.cpp
        src/ShaderReflectionSerializer.cpp
)

target_link_libraries(ShaderSystem PUBLIC Utils)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()