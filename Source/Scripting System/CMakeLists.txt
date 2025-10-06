cmake_minimum_required(VERSION 3.15)

project(Scripts)  # Add project name

# Set specific output directory for hot-reloading
set(SCRIPTS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/Scripts")

# Automatically gather all .cpp files in src/
file(GLOB_RECURSE SCRIPT_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/Assets/Scripts/*.cpp"
        "${CMAKE_SOURCE_DIR}/Source/Scripting System/EngineAPI/EngineAPI.cpp"
)

# Create the DLL (shared library)
add_library(Scripts SHARED ${SCRIPT_SOURCES})

# Define export macro for this target
target_compile_definitions(Scripts
        PRIVATE SCRIPTS_EXPORTS
)

# Set output directory for this target
set_target_properties(Scripts PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${SCRIPTS_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${SCRIPTS_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${SCRIPTS_OUTPUT_DIR}"
)

# Include dirs so scripts can find engine headers
target_include_directories(Scripts
        PUBLIC
        "${CMAKE_SOURCE_DIR}/Source/"
)

# Platform-specific settings for hot-reloading
if(WIN32)
    # On Windows, avoid locking the DLL
    set_target_properties(Scripts PROPERTIES
            SUFFIX ".dll"
    )
elseif(UNIX)
    # On Linux/macOS, use position-independent code
    set_target_properties(Scripts PROPERTIES
            POSITION_INDEPENDENT_CODE ON
            SUFFIX ".so"
    )
endif()