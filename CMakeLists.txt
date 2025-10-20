# ==============================
# Nous Engine - Root CMakeLists
# ==============================

cmake_minimum_required(VERSION 3.21...3.30)

# Modern CMake policies
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
# Includes (CMake modules)
# ==============================
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Config")
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/Functions")

include(GlobalSettings)
include(ProfilingOptions)

# ==============================
# Add Engine Sources
# ==============================
add_subdirectory(Source)

# ==============================
# Post-Build Tasks
# ==============================
include(ShaderCompilation)
include(AssetsCopy)
include(Messages)

setup_shader_compilation(EditorApp)
copy_assets(EditorApp)

# Optional: If we want to ensure scripts compile before run
add_dependencies(EditorApp Scripts)

# ==============================
# Summary
# ==============================
print_build_summary()