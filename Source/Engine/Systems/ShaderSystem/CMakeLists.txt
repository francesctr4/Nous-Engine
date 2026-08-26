# ==============================
# Nous Engine - Shader System
# ==============================

add_library(ShaderSystem STATIC)

add_subdirectory(ShaderCompiler)
add_subdirectory(ShaderReflection)
add_subdirectory(ShaderLoader)
add_subdirectory(ShaderParser)

set_target_properties(ShaderSystem PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(ShaderSystem PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(ShaderSystem PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(ShaderSystem PUBLIC
        unofficial::shaderc::shaderc
        unofficial::spirv-reflect
)

# ResourceManager closes a cycle: ResourceShader lives there, the compiler and
# reflection live here.
target_link_libraries(ShaderSystem PRIVATE
        FileSystem
        Logger
        MemoryManager
        ResourceManager
        Utils
)