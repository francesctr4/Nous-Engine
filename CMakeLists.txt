# ==============================
# Nous Engine - Root CMakeLists
# ==============================

cmake_minimum_required(VERSION 3.21...3.30)

# Modern CMake policies for consistent behavior
cmake_policy(SET CMP0077 NEW)
cmake_policy(SET CMP0091 NEW)
cmake_policy(SET CMP0111 NEW)

# ==============================
# Project Metadata
# ==============================
set(PROJECT_NAME "Nous-Engine")

project(${PROJECT_NAME}
        VERSION 0.4.0
        LANGUAGES CXX
        DESCRIPTION "A multithreaded, Vulkan-based game engine"
)

# ==============================
# Global Settings
# ==============================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

# Global Debug/Release definitions
add_compile_definitions(
        $$<CONFIG:Debug>:_DEBUG>
        $$<CONFIG:Release>:_RELEASE;NDEBUG>
)

option(ENABLE_PROFILING "Enable profiling instrumentation (Tracy, etc.)" OFF)

if(ENABLE_PROFILING)
    message(STATUS "Profiling enabled: Tracy instrumentation will be active.")
    add_compile_definitions(_PROFILING NDEBUG)
endif()

# ==============================
# Third-party Dependencies
# ==============================
find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(parson CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(glm REQUIRED)

# ==============================
# Engine Modules
# ==============================
add_subdirectory(Source/Engine/Core)
add_subdirectory(Source/Editor)
add_subdirectory(Source/Engine/Scripting)
add_subdirectory(Source/Engine/Multithreading)
add_subdirectory(Source/Engine/Utils)
add_subdirectory(Source/Engine/Renderer)
add_subdirectory(Source/Engine/ECS)
add_subdirectory(Source/Engine/Systems)

# ==============================
# Main Executable
# ==============================
add_executable(${PROJECT_NAME} Source/Engine/Core/Main.cpp)

target_include_directories(${PROJECT_NAME} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${STB_INCLUDE_PATH}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
        # Engine Modules
        NousEngine::Core
        NousEngine::Editor
        NousEngine::Multithreading
        NousEngine::Utils
        NousEngine::Renderer
        NousEngine::ECS
        NousEngine::Systems
        NousEngine::Scripting

        # Third-party Libraries
        SDL3::SDL3
        Vulkan::Vulkan
        assimp::assimp
        imgui::imgui
        parson::parson
        Tracy::TracyClient
        glm::glm
)

set_target_properties(${PROJECT_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        FOLDER "Executable"
)

# ==============================
# Shader Compilation (Cross-platform)
# ==============================
find_program(GLSLC_EXECUTABLE
        NAMES glslc
        HINTS ${Vulkan_GLSLC_EXECUTABLE}
        DOC "Path to the glslc shader compiler"
)

if(NOT GLSLC_EXECUTABLE)
    message(WARNING "glslc not found. Shaders will not be compiled automatically.")
else()
    message(STATUS "Found glslc: ${GLSLC_EXECUTABLE}")

    set(SHADER_SOURCE_DIR "${CMAKE_SOURCE_DIR}/Assets/Shaders")
    set(SHADER_BUILD_DIR "${CMAKE_BINARY_DIR}/Library/Shaders")

    file(GLOB_RECURSE SHADER_FILES CONFIGURE_DEPENDS
            "${SHADER_SOURCE_DIR}/*.glsl"
    )

    set(COMPILED_SHADERS)
    foreach(SHADER_FILE ${SHADER_FILES})
        file(RELATIVE_PATH SHADER_REL_PATH ${SHADER_SOURCE_DIR} ${SHADER_FILE})
        string(REGEX REPLACE "\\.glsl$" ".spv" SHADER_REL_PATH_SPV ${SHADER_REL_PATH})
        set(SHADER_OUTPUT_FILE "${SHADER_BUILD_DIR}/${SHADER_REL_PATH_SPV}")
        get_filename_component(SHADER_OUTPUT_DIR ${SHADER_OUTPUT_FILE} DIRECTORY)

        # Detect shader stage
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

        add_custom_command(
                OUTPUT ${SHADER_OUTPUT_FILE}
                COMMAND ${CMAKE_COMMAND} -E make_directory "${SHADER_OUTPUT_DIR}"
                COMMAND ${GLSLC_EXECUTABLE} -fshader-stage=${SHADER_STAGE} "${SHADER_FILE}" -o "${SHADER_OUTPUT_FILE}"
                DEPENDS ${SHADER_FILE}
                COMMENT "Compiling ${SHADER_STAGE} shader: ${SHADER_REL_PATH}"
                VERBATIM
        )

        list(APPEND COMPILED_SHADERS ${SHADER_OUTPUT_FILE})
    endforeach()

    if(COMPILED_SHADERS)
        add_custom_target(CompileShaders ALL
                DEPENDS ${COMPILED_SHADERS}
                COMMENT "Compiling all shaders"
        )
        add_dependencies(${PROJECT_NAME} CompileShaders)
    endif()
endif()

# ==============================
# Asset Copying
# ==============================
if(EXISTS ${CMAKE_SOURCE_DIR}/Assets)
    add_custom_target(CopyAssets ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Assets
            $<TARGET_FILE_DIR:${PROJECT_NAME}>/Assets
            COMMENT "Copying Assets directory"
    )
    add_dependencies(${PROJECT_NAME} CopyAssets)
endif()

# ==============================
# Scripts DLL Dependency
# ==============================
add_dependencies(${PROJECT_NAME} Scripts)

# ==============================
# Output Configuration Summary
# ==============================
message(STATUS "")
message(STATUS "==============================")
message(STATUS " Nous Engine Build Configuration")
message(STATUS "==============================")
message(STATUS " Project:          ${PROJECT_NAME}")
message(STATUS " C++ Standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS " Build Type:       ${CMAKE_BUILD_TYPE}")
message(STATUS " Output Directory: ${CMAKE_BINARY_DIR}")
message(STATUS "==============================")
message(STATUS "")