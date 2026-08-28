# ==============================================================================
# Nous Engine - Shader System
#
# Public/private header layout (see CONVENTIONS.md).
#
#   include/ShaderSystem/**  is the API, e.g. <ShaderSystem/ShaderTypes.h>.
#   src/**                   is implementation.
#
# Every one of the nine headers has a consumer outside this target (the renderer
# takes ShaderLoader/ShaderCompiler types, ImporterShader drives the compiler,
# parser and reflection, ResourceMaterial and the inspector name the reflection
# types), so nothing moved to src/ and this conversion buys no new enforcement
# here -- only the <ShaderSystem/...> prefix and the two handles. That is an
# honest outcome, not an oversight: this target really is all API. The check was
# a grep for includers outside the target, same as everywhere else.
# ==============================================================================

set(CLASS_NAME "ShaderSystem")

add_library(${CLASS_NAME}_headers INTERFACE)
target_include_directories(${CLASS_NAME}_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(${CLASS_NAME}_private INTERFACE)
target_include_directories(${CLASS_NAME}_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(${CLASS_NAME} STATIC)

target_sources(${CLASS_NAME} PRIVATE
        include/${CLASS_NAME}/ShaderTypes.h
        include/${CLASS_NAME}/ShaderCompiler/ShaderCompiler.h
        include/${CLASS_NAME}/ShaderCompiler/ShaderCompilerTypes.h
        include/${CLASS_NAME}/ShaderLoader/ShaderLoader.h
        include/${CLASS_NAME}/ShaderLoader/ShaderLoaderTypes.h
        include/${CLASS_NAME}/ShaderParser/ShaderParser.h
        include/${CLASS_NAME}/ShaderReflection/ShaderReflection.h
        include/${CLASS_NAME}/ShaderReflection/ShaderReflectionSerializer.h
        include/${CLASS_NAME}/ShaderReflection/ShaderReflectionTypes.h
)

set_target_properties(${CLASS_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

target_include_directories(${CLASS_NAME}
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(${CLASS_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(${CLASS_NAME} PUBLIC
        ${CLASS_NAME}_headers
        unofficial::shaderc::shaderc
        unofficial::spirv-reflect
)

# ResourceManager closes a cycle: ResourceShader lives there, the compiler and
# reflection live here.
target_link_libraries(${CLASS_NAME} PRIVATE
        FileSystem
        Logger
        MemoryManager
        ResourceManager
        Utils
)

add_subdirectory(src/ShaderCompiler)
add_subdirectory(src/ShaderReflection)
add_subdirectory(src/ShaderLoader)
add_subdirectory(src/ShaderParser)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
