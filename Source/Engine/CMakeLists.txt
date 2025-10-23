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
add_subdirectory(Core)
add_subdirectory(Scripting)
add_subdirectory(NOUS_Multithreading)
add_subdirectory(Utils)
add_subdirectory(Renderer)
add_subdirectory(ECS)
add_subdirectory(Systems)

# ==============================
# NousEngine DLL (Shared Library)
# ==============================

add_library(${PROJECT_NAME} SHARED MainEngine.cpp)
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
        $<TARGET_OBJECTS:Renderer>
        $<TARGET_OBJECTS:ECS>
        $<TARGET_OBJECTS:Scripting>
)

# Link engine modules + dependencies
target_link_libraries(${PROJECT_NAME} PRIVATE
        # Third-Party
        SDL3::SDL3
        Vulkan::Vulkan
        assimp::assimp
        parson::parson
        Tracy::TracyClient
        glm::glm
        # Engine
        NOUS_Multithreading
)

set_target_properties(${PROJECT_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
        FOLDER "Engine"
)