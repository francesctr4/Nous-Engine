cmake_minimum_required(VERSION 3.21)

set(PROJECT_NAME "Nous-Engine")
set(PROJECT_VERSION_MAJOR 0)
set(PROJECT_VERSION_MINOR 4)
set(PROJECT_VERSION_PATCH 0)

project(${PROJECT_NAME} 
    VERSION ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}
    LANGUAGES CXX
    DESCRIPTION "A multithreaded, Vulkan-based game engine"
)

# Define ALL_SOURCES once
set(ALL_SOURCES
        Source/Core/Main.cpp

        "Source/Scripting/Internal/IScript.inl"
        "Source/Scripting/Internal/ScriptTemplate.inl"
        "Source/Scripting/Internal/ScriptRegistry.inl"
        "Source/Scripting/ScriptManager.h"
        "Source/Scripting/ScriptManager.cpp"
        "Source/Scripting/EngineAPI/ScriptBindings.h"
        "Source/Scripting/EngineAPI/ScriptBindings.cpp"
        "Source/Scripting/EngineAPI/EngineAPI.cpp"
        "Source/Scripting/EngineAPI/Bindings/InputBindings.cpp"
        "Source/Scripting/EngineAPI/Bindings/InputBindings.h"
        "Source/Scripting/EngineAPI/Bindings/LoggerBindings.cpp"
        "Source/Scripting/EngineAPI/Bindings/LoggerBindings.h"
        "Source/Scripting/EngineAPI/Bindings/GameObjectBindings.cpp"
        "Source/Scripting/EngineAPI/Bindings/GameObjectBindings.h"
)

add_subdirectory(Source/Core)
add_subdirectory(Source/Editor)
add_subdirectory(Source/Scripting)
add_subdirectory(Source/Multithreading)
add_subdirectory(Source/Utils)
add_subdirectory(Source/Renderer)
add_subdirectory(Source/ECS)
add_subdirectory(Source/Systems)

add_executable(${PROJECT_NAME} ${ALL_SOURCES})

set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# Debug/Release configurations (works for all generators)
target_compile_definitions(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Release>:_RELEASE;NDEBUG>
)

if (ENABLE_PROFILING)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _PROFILING NDEBUG)
endif()

# Find dependencies
find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(parson CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(glm REQUIRED)

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source/  # Adjust if needed
        ${STB_INCLUDE_PATH}          # Explicit path to STB
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        # Engine
        NousEngine::Core
        NousEngine::Editor
        NousEngine::Multithreading
        NousEngine::Utils
        NousEngine::Renderer
        NousEngine::ECS
        NousEngine::Systems
        # Third Party
        SDL3::SDL3
        Vulkan::Vulkan
        assimp::assimp
        imgui::imgui
        parson::parson
        Tracy::TracyClient
        glm::glm
)

# IDE Source grouping
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/Source" PREFIX "Source Files" FILES ${ALL_SOURCES})
set_property(GLOBAL PROPERTY USE_FOLDERS TRUE)
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

#----------------------------------------------------------------
# Shader Compilation with CMake (Cross-platform)
#----------------------------------------------------------------

# Find the Vulkan shader compiler
find_program(GLSLC_EXECUTABLE
        NAMES glslc
        HINTS ${Vulkan_GLSLC_EXECUTABLE}
        DOC "Path to the glslc shader compiler"
)

if(NOT GLSLC_EXECUTABLE)
    message(WARNING "glslc not found. Shaders will not be compiled automatically.")
else()
    message(STATUS "Found glslc: ${GLSLC_EXECUTABLE}")

    # Define shader directories
    set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Assets/Shaders")
    set(SHADER_BUILD_DIR "${CMAKE_BINARY_DIR}/Library/Shaders")

    # Find all shader files with .glsl extension
    file(GLOB_RECURSE SHADER_FILES
            "${SHADER_SOURCE_DIR}/*.glsl"
    )

    # Create shader compilation targets
    set(COMPILED_SHADERS)
    foreach(SHADER_FILE ${SHADER_FILES})
        # Get relative path and create output path
        file(RELATIVE_PATH SHADER_REL_PATH ${SHADER_SOURCE_DIR} ${SHADER_FILE})
        # Remove .glsl and add .spv
        string(REGEX REPLACE "\\.glsl$" ".spv" SHADER_REL_PATH_SPV ${SHADER_REL_PATH})
        set(SHADER_OUTPUT_FILE "${SHADER_BUILD_DIR}/${SHADER_REL_PATH_SPV}")

        # Get directory of output file and create it
        get_filename_component(SHADER_OUTPUT_DIR ${SHADER_OUTPUT_FILE} DIRECTORY)

        # Determine shader stage from filename
        set(SHADER_STAGE "")
        if(SHADER_REL_PATH MATCHES "\\.vert\\.")
            set(SHADER_STAGE "vertex")
        elseif(SHADER_REL_PATH MATCHES "\\.frag\\.")
            set(SHADER_STAGE "fragment")
        elseif(SHADER_REL_PATH MATCHES "\\.comp\\.")
            set(SHADER_STAGE "compute")
        elseif(SHADER_REL_PATH MATCHES "\\.geom\\.")
            set(SHADER_STAGE "geometry")
        elseif(SHADER_REL_PATH MATCHES "\\.tesc\\.")
            set(SHADER_STAGE "tesscontrol")
        elseif(SHADER_REL_PATH MATCHES "\\.tese\\.")
            set(SHADER_STAGE "tesseval")
        else()
            message(WARNING "Unknown shader stage for ${SHADER_REL_PATH}. Skipping...")
            continue()
        endif()

        # Create custom command for each shader
        add_custom_command(
                OUTPUT ${SHADER_OUTPUT_FILE}
                COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
                COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=${SHADER_STAGE} "${SHADER_FILE}" -o "${SHADER_OUTPUT_FILE}"
                DEPENDS ${SHADER_FILE}
                COMMENT "Compiling ${SHADER_STAGE} shader: ${SHADER_REL_PATH} -> ${SHADER_REL_PATH_SPV}"
                VERBATIM
        )

        list(APPEND COMPILED_SHADERS ${SHADER_OUTPUT_FILE})
    endforeach()

    # Create a target that compiles all shaders
    if(COMPILED_SHADERS)
        add_custom_target(CompileShaders ALL
                DEPENDS ${COMPILED_SHADERS}
                COMMENT "Compiling all shaders"
        )
        add_dependencies(${PROJECT_NAME} CompileShaders)
    endif()
endif()

#----------------------------------------------------------------
# Copy assets to build directory (handles updates)
#----------------------------------------------------------------
if(EXISTS ${CMAKE_SOURCE_DIR}/Assets)
    add_custom_target(CopyAssets ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Assets
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets
            COMMENT "Updating assets directory"
            VERBATIM
    )
    add_dependencies(${PROJECT_NAME} CopyAssets)
else()
    message(WARNING "Assets directory not found: ${CMAKE_SOURCE_DIR}/Assets")
endif()

#----------------------------------------------------------------
# Scripts Library Configuration
#----------------------------------------------------------------
add_dependencies(${PROJECT_NAME} Scripts)