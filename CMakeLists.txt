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
        "Source/Core/Application.cpp"
        "Source/Core/Application.h"
        "Source/Core/Globals.h"
        "Source/Core/Main.cpp"

        "Source/Data Structures/DynamicArray.h"

        "Source/Editor/ImGuiConfig/ImGuiCustom.cpp"
        "Source/Editor/ImGuiConfig/ImGuiCustom.h"
        "Source/Editor/Windows/AssetsBrowser.cpp"
        "Source/Editor/Windows/AssetsBrowser.h"
        "Source/Editor/Windows/GameViewport.cpp"
        "Source/Editor/Windows/GameViewport.h"
        "Source/Editor/Windows/JobQueueWindow.cpp"
        "Source/Editor/Windows/JobQueueWindow.h"
        "Source/Editor/Windows/MainMenuBar.cpp"
        "Source/Editor/Windows/MainMenuBar.h"
        "Source/Editor/Windows/MultithreadingWindow.cpp"
        "Source/Editor/Windows/MultithreadingWindow.h"
        "Source/Editor/Windows/ResourcesWindow.cpp"
        "Source/Editor/Windows/ResourcesWindow.h"
        "Source/Editor/Windows/SceneViewport.cpp"
        "Source/Editor/Windows/SceneViewport.h"
        "Source/Editor/IEditorWindow.inl"

        "Source/Includes/Assimp.h"
        "Source/Includes/ImGui.h"
        "Source/Includes/glmath.h"
        "Source/Includes/SDL3.h"
        "Source/Includes/Tracy.h"
        "Source/Includes/Vulkan.h"
        "Source/Includes/Stb.h"
        "Source/Includes/Parson.h"

        "Source/Modules/Module.cpp"
        "Source/Modules/Module.h"
        "Source/Modules/ModuleCamera3D.cpp"
        "Source/Modules/ModuleCamera3D.h"
        "Source/Modules/ModuleEditor.cpp"
        "Source/Modules/ModuleEditor.h"
        "Source/Modules/ModuleFileSystem.cpp"
        "Source/Modules/ModuleFileSystem.h"
        "Source/Modules/ModuleInput.cpp"
        "Source/Modules/ModuleInput.h"
        "Source/Modules/ModuleRenderer3D.cpp"
        "Source/Modules/ModuleRenderer3D.h"
        "Source/Modules/ModuleResourceManager.cpp"
        "Source/Modules/ModuleResourceManager.h"
        "Source/Modules/ModuleScene.cpp"
        "Source/Modules/ModuleScene.h"
        "Source/Modules/ModuleWindow.cpp"
        "Source/Modules/ModuleWindow.h"

        "Source/Multithreading/NOUS_Job.cpp"
        "Source/Multithreading/NOUS_Job.h"
        "Source/Multithreading/NOUS_JobSystem.cpp"
        "Source/Multithreading/NOUS_JobSystem.h"
        "Source/Multithreading/NOUS_Multithreading.cpp"
        "Source/Multithreading/NOUS_Multithreading.h"
        "Source/Multithreading/NOUS_Thread.cpp"
        "Source/Multithreading/NOUS_Thread.h"
        "Source/Multithreading/NOUS_ThreadPool.cpp"
        "Source/Multithreading/NOUS_ThreadPool.h"

        "Source/Renderer/RendererBackend.cpp"
        "Source/Renderer/RendererBackend.h"
        "Source/Renderer/RendererFrontend.cpp"
        "Source/Renderer/RendererFrontend.h"
        "Source/Renderer/RendererTypes.inl"

        "Source/Renderer/Vulkan/Shaders/VulkanMaterialShader.cpp"
        "Source/Renderer/Vulkan/Shaders/VulkanMaterialShader.h"
        "Source/Renderer/Vulkan/Shaders/VulkanShaderUtils.cpp"
        "Source/Renderer/Vulkan/Shaders/VulkanShaderUtils.h"
        "Source/Renderer/Vulkan/Shaders/VulkanUIShader.cpp"
        "Source/Renderer/Vulkan/Shaders/VulkanUIShader.h"

        "Source/Renderer/Vulkan/VulkanBackend.cpp"
        "Source/Renderer/Vulkan/VulkanBackend.h"
        "Source/Renderer/Vulkan/VulkanBuffer.cpp"
        "Source/Renderer/Vulkan/VulkanBuffer.h"
        "Source/Renderer/Vulkan/VulkanCommandBuffer.cpp"
        "Source/Renderer/Vulkan/VulkanCommandBuffer.h"
        "Source/Renderer/Vulkan/VulkanDebugMessenger.cpp"
        "Source/Renderer/Vulkan/VulkanDebugMessenger.h"
        "Source/Renderer/Vulkan/VulkanDevice.cpp"
        "Source/Renderer/Vulkan/VulkanDevice.h"
        "Source/Renderer/Vulkan/VulkanExternal.cpp"
        "Source/Renderer/Vulkan/VulkanExternal.h"
        "Source/Renderer/Vulkan/VulkanFramebuffer.cpp"
        "Source/Renderer/Vulkan/VulkanFramebuffer.h"
        "Source/Renderer/Vulkan/VulkanGlobals.h"
        "Source/Renderer/Vulkan/VulkanGraphicsPipeline.cpp"
        "Source/Renderer/Vulkan/VulkanGraphicsPipeline.h"
        "Source/Renderer/Vulkan/VulkanImage.cpp"
        "Source/Renderer/Vulkan/VulkanImage.h"
        "Source/Renderer/Vulkan/VulkanImGuiResources.cpp"
        "Source/Renderer/Vulkan/VulkanImGuiResources.h"
        "Source/Renderer/Vulkan/VulkanInstance.cpp"
        "Source/Renderer/Vulkan/VulkanInstance.h"
        "Source/Renderer/Vulkan/VulkanMultithreading.cpp"
        "Source/Renderer/Vulkan/VulkanMultithreading.h"
        "Source/Renderer/Vulkan/VulkanRenderpass.cpp"
        "Source/Renderer/Vulkan/VulkanRenderpass.h"
        "Source/Renderer/Vulkan/VulkanSwapchain.cpp"
        "Source/Renderer/Vulkan/VulkanSwapchain.h"
        "Source/Renderer/Vulkan/VulkanSyncObjects.cpp"
        "Source/Renderer/Vulkan/VulkanSyncObjects.h"
        "Source/Renderer/Vulkan/VulkanTypes.inl"
        "Source/Renderer/Vulkan/VulkanUtils.cpp"
        "Source/Renderer/Vulkan/VulkanUtils.h"

        "Source/Systems/Camera System/Camera.cpp"
        "Source/Systems/Camera System/Camera.h"

        "Source/Systems/Event System/EventSystem.h"

        "Source/Systems/File System/FileHandle.cpp"
        "Source/Systems/File System/FileHandle.h"
        "Source/Systems/File System/FileManager.cpp"
        "Source/Systems/File System/FileManager.h"

        "Source/Systems/Geometry System/GeometrySystem.cpp"
        "Source/Systems/Geometry System/GeometrySystem.h"
        "Source/Systems/Geometry System/Vertex.inl"

        "Source/Systems/Material System/MaterialSystem.cpp"
        "Source/Systems/Material System/MaterialSystem.h"

        "Source/Systems/Memory Manager/MemoryManager.cpp"
        "Source/Systems/Memory Manager/MemoryManager.h"

        "Source/Systems/Memory Manager/Custom Allocators/DynamicAllocator.cpp"
        "Source/Systems/Memory Manager/Custom Allocators/DynamicAllocator.h"
        "Source/Systems/Memory Manager/Custom Allocators/FreeList.cpp"
        "Source/Systems/Memory Manager/Custom Allocators/FreeList.h"
        "Source/Systems/Memory Manager/Custom Allocators/LinearAllocator.cpp"
        "Source/Systems/Memory Manager/Custom Allocators/LinearAllocator.h"

        "Source/Systems/Resource Manager/MetaFileData.inl"

        "Source/Systems/Resource Manager/Importers/Importer.inl"
        "Source/Systems/Resource Manager/Importers/ImporterManager.cpp"
        "Source/Systems/Resource Manager/Importers/ImporterManager.h"
        "Source/Systems/Resource Manager/Importers/ImporterMaterial.cpp"
        "Source/Systems/Resource Manager/Importers/ImporterMaterial.h"
        "Source/Systems/Resource Manager/Importers/ImporterMesh.cpp"
        "Source/Systems/Resource Manager/Importers/ImporterMesh.h"
        "Source/Systems/Resource Manager/Importers/ImporterTexture.cpp"
        "Source/Systems/Resource Manager/Importers/ImporterTexture.h"

        "Source/Systems/Resource Manager/Resource Types/Resource.cpp"
        "Source/Systems/Resource Manager/Resource Types/Resource.h"
        "Source/Systems/Resource Manager/Resource Types/ResourceMaterial.cpp"
        "Source/Systems/Resource Manager/Resource Types/ResourceMaterial.h"
        "Source/Systems/Resource Manager/Resource Types/ResourceMesh.cpp"
        "Source/Systems/Resource Manager/Resource Types/ResourceMesh.h"
        "Source/Systems/Resource Manager/Resource Types/ResourceTexture.cpp"
        "Source/Systems/Resource Manager/Resource Types/ResourceTexture.h"

        "Source/Systems/Texture System/TextureSystem.cpp"
        "Source/Systems/Texture System/TextureSystem.h"

        "Source/Systems/Time Management/TimeManager.h"
        "Source/Systems/Time Management/Timer.cpp"
        "Source/Systems/Time Management/Timer.h"

        "Source/Utils/Asserts.h"
        "Source/Utils/JsonFile.cpp"
        "Source/Utils/JsonFile.h"
        "Source/Utils/Logger.cpp"
        "Source/Utils/Logger.h"
        "Source/Utils/MathUtils.h"
        "Source/Utils/Random.cpp"
        "Source/Utils/Random.h"
        "Source/Scripting System/IScript.inl"
        "Source/Scripting System/ScriptTemplate.inl"
        "Source/Scripting System/ScriptRegistry.h"
        "Source/Scripting System/ScriptRegistryExport.h"
        "Source/Scripting System/ScriptRegistration.h"
        "Source/Scripting System/ScriptRegistry.cpp"
        "Source/Scripting System/ScriptRegistryExport.cpp"
)

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
add_subdirectory("Source/Scripting System")