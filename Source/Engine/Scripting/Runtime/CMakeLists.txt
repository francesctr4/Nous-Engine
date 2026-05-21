# ==============================
# Nous Engine - Scripts DLL (Hot Reload)
# ==============================
#
# Directory layout (next to the engine executable):
#
#   <bin>/Library/Scripts/Scripts.dll   ← BUILD OUTPUT (user-deletable; regenerable)
#   <bin>/EngineCore/Scripts/           ← BUILD TOOLING (must survive Library deletion)
#       ├── RebuildScripts.bat / .sh    ← script that compiles Scripts.dll
#       ├── ScriptTemplate.inl          ← template used when creating a new script
#       └── SDK/                        ← engine headers + EngineAPI.cpp the bat consumes
#
# Splitting tooling out of Library/ lets the engine regenerate Scripts.dll
# when the user wipes Library/ — otherwise the bat itself is deleted along
# with everything else and there's nothing left to rebuild from.

set(SCRIPTS_OUTPUT_DIR   "${CMAKE_BINARY_DIR}/bin/Library/Scripts")
set(SCRIPTS_TOOLING_DIR  "${CMAKE_BINARY_DIR}/bin/EngineCore/Scripts")
set(SCRIPTS_SRC_DIR      "${CMAKE_SOURCE_DIR}/Assets")

file(MAKE_DIRECTORY "${SCRIPTS_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${SCRIPTS_TOOLING_DIR}")

if(WIN32)
    configure_file(
            "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/Runtime/RebuildScripts.bat.in"
            "${SCRIPTS_TOOLING_DIR}/RebuildScripts.bat"
            @ONLY
    )
else()
    configure_file(
            "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/Runtime/RebuildScripts.sh.in"
            "${SCRIPTS_TOOLING_DIR}/RebuildScripts.sh"
            @ONLY
    )
    # Ensure the script is executable after configuration
    file(CHMOD "${SCRIPTS_TOOLING_DIR}/RebuildScripts.sh"
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                        GROUP_READ GROUP_EXECUTE
                        WORLD_READ WORLD_EXECUTE
    )
endif()
file(COPY "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/Internal/ScriptTemplate.inl"
        DESTINATION "${SCRIPTS_TOOLING_DIR}")

set(SCRIPTS_SDK_DIR "${SCRIPTS_TOOLING_DIR}/SDK")
set(SCRIPTS_SDK_INCLUDE_DIR "${SCRIPTS_SDK_DIR}/include")
set(SCRIPTS_SDK_SRC_DIR "${SCRIPTS_SDK_DIR}/src")

file(MAKE_DIRECTORY
        "${SCRIPTS_SDK_INCLUDE_DIR}/Engine/Scripting/Internal"
        "${SCRIPTS_SDK_INCLUDE_DIR}/Engine/Scripting/EngineAPI"
)

file(COPY
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/Internal/IScript.inl"
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/Internal/ScriptRegistry.inl"
        DESTINATION
        "${SCRIPTS_SDK_INCLUDE_DIR}/Engine/Scripting/Internal"
)

file(COPY
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/EngineAPI/EngineAPI.h"
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/EngineAPI/NOUS_SCANCODE.h"
        DESTINATION
        "${SCRIPTS_SDK_INCLUDE_DIR}/Engine/Scripting/EngineAPI"
)

file(COPY
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/EngineAPI/EngineAPI.cpp"
        DESTINATION
        "${SCRIPTS_SDK_SRC_DIR}"
)

# Copy ALL headers under EngineAPI/Bindings preserving folders
file(GLOB_RECURSE SDK_BINDINGS_HEADERS
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/EngineAPI/Bindings/*.h"
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/EngineAPI/Bindings/*.inl"
)

foreach(hdr IN LISTS SDK_BINDINGS_HEADERS)
    file(RELATIVE_PATH rel "${CMAKE_SOURCE_DIR}/Source" "${hdr}")
    get_filename_component(rel_dir "${rel}" DIRECTORY)
    file(MAKE_DIRECTORY "${SCRIPTS_SDK_INCLUDE_DIR}/${rel_dir}")
    file(COPY "${hdr}" DESTINATION "${SCRIPTS_SDK_INCLUDE_DIR}/${rel_dir}")
endforeach()

# Discover script sources from source Assets/Scripts — edits there take effect immediately
file(GLOB_RECURSE SCRIPT_SOURCES
        CONFIGURE_DEPENDS
        "${SCRIPTS_SRC_DIR}/*.cpp"
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/EngineAPI/EngineAPI.cpp"
)

add_library(Scripts SHARED ${SCRIPT_SOURCES})
add_library(NousEngine::Scripts ALIAS Scripts)

target_compile_definitions(Scripts PRIVATE SCRIPTS_EXPORTS)

# Output folder for DLL
set_target_properties(Scripts PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${SCRIPTS_OUTPUT_DIR}"
        LIBRARY_OUTPUT_DIRECTORY "${SCRIPTS_OUTPUT_DIR}"
        ARCHIVE_OUTPUT_DIRECTORY "${SCRIPTS_OUTPUT_DIR}"
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

# Include engine headers + source scripts dir
target_include_directories(Scripts PUBLIC
        "${CMAKE_SOURCE_DIR}/Source/"
        "${SCRIPTS_SRC_DIR}"
)

# Platform specifics
if(WIN32)
    set_target_properties(Scripts PROPERTIES SUFFIX ".dll")
elseif(UNIX)
    set_target_properties(Scripts PROPERTIES
            POSITION_INDEPENDENT_CODE ON
            SUFFIX ".so"
    )
endif()