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
set(EDITOR_NAME "Nous-Editor")

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

# Build config macros
add_compile_definitions(
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Release>:_RELEASE>
        $<$<CONFIG:Release>:NDEBUG>
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
find_package(parson CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(glm REQUIRED)

# ==============================
# Engine Modules
# ==============================
add_subdirectory(Source/Engine/Core)
add_subdirectory(Source/Engine/Scripting)
add_subdirectory(Source/Engine/Multithreading)
add_subdirectory(Source/Engine/Utils)
add_subdirectory(Source/Engine/Renderer)
add_subdirectory(Source/Engine/ECS)
add_subdirectory(Source/Engine/Systems)

add_subdirectory(Source/Editor)
add_dependencies(Nous-Editor ${PROJECT_NAME})

# ==============================
# NousEngine DLL (Shared Library)
# ==============================

add_library(${PROJECT_NAME} SHARED Source/Engine/MainEngine.cpp)
add_library(NousEngine::Engine ALIAS ${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

# Include directories
target_include_directories(${PROJECT_NAME}
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

target_sources(${PROJECT_NAME} PRIVATE
        $<TARGET_OBJECTS:Core>
        $<TARGET_OBJECTS:Modules>
        $<TARGET_OBJECTS:Utils>
        $<TARGET_OBJECTS:Systems>
        $<TARGET_OBJECTS:Multithreading>
        $<TARGET_OBJECTS:Renderer>
        $<TARGET_OBJECTS:ECS>
        $<TARGET_OBJECTS:Scripting>
)

# Link engine modules + dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE
        SDL3::SDL3
        Vulkan::Vulkan
        assimp::assimp
        parson::parson
        Tracy::TracyClient
        glm::glm
)

set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        FOLDER "Engine"
)

# ==============================
# Editor Executable
# ==============================
add_executable(EditorApp Source/Editor/MainEditor.cpp)

target_include_directories(EditorApp PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/Source
        ${STB_INCLUDE_PATH}
)

target_link_libraries(EditorApp PRIVATE
        NousEngine::Engine
        NousEngine::Editor
)

set_target_properties(EditorApp PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        FOLDER "Applications"
)

# =========================================================
# Game Runtime Executable
# =========================================================
add_executable(GameApp
        Source/Game/MainGame.cpp
)

target_link_libraries(GameApp PRIVATE
        NousEngine::Engine
)

# Output to the same bin folder for consistency
set_target_properties(GameApp PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
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
    set(SHADER_BUILD_DIR "${CMAKE_BINARY_DIR}/bin/Library/Shaders")

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
        add_dependencies(EditorApp CompileShaders)
    endif()
endif()

# ==============================
# Asset Copying
# ==============================
if(EXISTS ${CMAKE_SOURCE_DIR}/Assets)
    add_custom_target(CopyAssets ALL
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_SOURCE_DIR}/Assets
            ${CMAKE_BINARY_DIR}/bin/Assets
            COMMENT "Copying Assets directory"
    )
    add_dependencies(EditorApp CopyAssets)
endif()

# ==============================
# Scripts DLL Dependency
# ==============================
add_dependencies(EditorApp Scripts)

# ==============================
# Output Configuration Summary
# ==============================
message(STATUS "")
message(STATUS "==============================")
message(STATUS " Nous Engine Build Configuration")
message(STATUS "==============================")
message(STATUS " Engine DLL:       NousEngine.dll")
message(STATUS " Editor Executable:EditorApp.exe")
message(STATUS " C++ Standard:     ${CMAKE_CXX_STANDARD}")
message(STATUS " Build Type:       ${CMAKE_BUILD_TYPE}")
message(STATUS " Output Directory: ${CMAKE_BINARY_DIR}/bin")
message(STATUS "==============================")
message(STATUS "")