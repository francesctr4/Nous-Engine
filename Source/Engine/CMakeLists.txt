# ==============================
# Third-party Dependencies
# ==============================
find_package(SDL3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_package(assimp CONFIG REQUIRED)
find_package(parson CONFIG REQUIRED)
find_package(Tracy CONFIG REQUIRED)
find_package(glm REQUIRED)
find_package(magic_enum CONFIG REQUIRED)
find_package(unofficial-shaderc CONFIG REQUIRED)
find_package(unofficial-spirv-reflect CONFIG REQUIRED)

# ==============================
# Engine Modules
# ==============================
add_subdirectory(Core)
add_subdirectory(Modules)
add_subdirectory(Scripting)
add_subdirectory(NOUS_Multithreading)
add_subdirectory(Utils)
add_subdirectory(Renderer)
add_subdirectory(Systems)

# ==============================
# NousEngine DLL (Shared Library)
# ==============================

add_library(${PROJECT_NAME} SHARED EngineExport.h MainEngine.cpp)
add_library(NousEngine::Engine ALIAS ${PROJECT_NAME})

target_compile_definitions(${PROJECT_NAME} PRIVATE NOUS_ENGINE_EXPORTS)

# Include directories
target_include_directories(${PROJECT_NAME}
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/Source
)

# Link engine modules + dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE
        # --- Third-Party ---
        SDL3::SDL3
        Vulkan::Vulkan
        assimp::assimp
        parson::parson
        Tracy::TracyClient
        glm::glm

        # --- Engine Modules ---
        Core
        MemoryManager
        Modules
        NOUS_Multithreading
        Renderer
        Scripting
        Systems
        Utils
)

if (WIN32)
  target_link_options(${PROJECT_NAME} PRIVATE
    "LINKER:/WHOLEARCHIVE:$<TARGET_FILE:Core>"
    "LINKER:/WHOLEARCHIVE:$<TARGET_FILE:MemoryManager>"
    "LINKER:/WHOLEARCHIVE:$<TARGET_FILE:Modules>"
    "LINKER:/WHOLEARCHIVE:$<TARGET_FILE:NOUS_Multithreading>"
    "LINKER:/WHOLEARCHIVE:$<TARGET_FILE:Renderer>"
    "LINKER:/WHOLEARCHIVE:$<TARGET_FILE:Scripting>"
    "LINKER:/WHOLEARCHIVE:$<TARGET_FILE:Systems>"
    "LINKER:/WHOLEARCHIVE:$<TARGET_FILE:Utils>"
  )
endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        FOLDER "Engine"
)