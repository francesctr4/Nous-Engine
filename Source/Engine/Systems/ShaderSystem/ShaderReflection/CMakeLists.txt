target_sources(ShaderSystem PRIVATE
        include/ShaderReflection.h
        include/ShaderReflectionTypes.h
        include/ShaderReflectionSerializer.h
        src/ShaderReflection.cpp
        src/ShaderReflectionSerializer.cpp
)

find_package(parson CONFIG REQUIRED)
target_link_libraries(ShaderSystem PUBLIC parson::parson)