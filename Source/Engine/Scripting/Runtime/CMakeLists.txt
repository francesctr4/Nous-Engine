# ==============================
# Nous Engine - Scripts DLL (Hot Reload)
# ==============================
#
# Directory layout (next to the engine executable):
#
#   <bin>/Library/Scripts/Scripts.dll   ← BUILD OUTPUT (user-deletable; regenerable)
#   <bin>/EngineSDK/Scripts/           ← BUILD TOOLING (must survive Library deletion)
#
# Since the header-layout conversion the SDK include tree is a VERBATIM COPY of
# Scripting/include/, so a user script includes exactly what an engine TU does:
#   #include <Scripting/EngineAPI/EngineAPI.h>
# Keep it that way -- if the two ever diverge, a script compiles in-tree and
# fails on hot-reload (or the reverse), which is very hard to diagnose.
#       ├── RebuildScripts.bat / .sh    ← script that compiles Scripts.dll
#       ├── ScriptTemplate.inl          ← template used when creating a new script
#       └── SDK/                        ← engine headers + EngineAPI.cpp the bat consumes
#
# Splitting tooling out of Library/ lets the engine regenerate Scripts.dll
# when the user wipes Library/ — otherwise the bat itself is deleted along
# with everything else and there's nothing left to rebuild from.

set(SCRIPTS_OUTPUT_DIR   "${CMAKE_BINARY_DIR}/bin/Library/Scripts")
set(SCRIPTS_TOOLING_DIR  "${CMAKE_BINARY_DIR}/bin/EngineSDK/Scripts")
set(SCRIPTS_SRC_DIR      "${CMAKE_SOURCE_DIR}/Assets")

file(MAKE_DIRECTORY "${SCRIPTS_OUTPUT_DIR}")
file(MAKE_DIRECTORY "${SCRIPTS_TOOLING_DIR}")

# Sanitizer flags for the HOT-RELOAD compile line.
#
# The `Scripts` CMake target below inherits the global flags from Sanitizers.cmake
# automatically, but ScriptManager::RecompileScripts does not go through CMake — it
# shells out to the generated RebuildScripts script, which builds its own flag list.
# Without this, hot-reloading a script during a sanitizer session silently swapped in
# an UNINSTRUMENTED Scripts.so, and accesses made inside script code stopped being
# tracked. Substituted into the script as @NOUS_SCRIPTS_SANITIZE_FLAGS@.
#
# Empty unless NOUS_SANITIZE is set, so Release and shipped builds are unaffected.
if(NOUS_SANITIZE AND NOT MSVC)
    set(NOUS_SCRIPTS_SANITIZE_FLAGS "-fsanitize=${NOUS_SANITIZE} -fno-omit-frame-pointer")
else()
    set(NOUS_SCRIPTS_SANITIZE_FLAGS "")
endif()

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
            # LF explicitly: configure_file preserves the input's line endings,
            # and the .in is edited on Windows. A CRLF shebang fails on Linux as
            # "/usr/bin/env bash^M: bad interpreter", an obscure way to discover
            # that script hot-reload is broken.
            NEWLINE_STYLE LF
    )
    # Ensure the script is executable after configuration
    file(CHMOD "${SCRIPTS_TOOLING_DIR}/RebuildScripts.sh"
            PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE
                        GROUP_READ GROUP_EXECUTE
                        WORLD_READ WORLD_EXECUTE
    )
endif()
file(COPY "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/include/Scripting/Internal/ScriptTemplate.inl"
        DESTINATION "${SCRIPTS_TOOLING_DIR}")

set(SCRIPTS_SDK_DIR "${SCRIPTS_TOOLING_DIR}/SDK")
set(SCRIPTS_SDK_INCLUDE_DIR "${SCRIPTS_SDK_DIR}/include")
set(SCRIPTS_SDK_SRC_DIR "${SCRIPTS_SDK_DIR}/src")

file(MAKE_DIRECTORY
        "${SCRIPTS_SDK_INCLUDE_DIR}/Scripting/Internal"
        "${SCRIPTS_SDK_INCLUDE_DIR}/Scripting/EngineAPI"
        "${SCRIPTS_SDK_INCLUDE_DIR}/Scripting/EngineAPI/Bindings"
)

file(COPY
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/include/Scripting/Internal/IScript.inl"
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/include/Scripting/Internal/ScriptRegistry.inl"
        DESTINATION
        "${SCRIPTS_SDK_INCLUDE_DIR}/Scripting/Internal"
)

file(COPY
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/include/Scripting/EngineAPI/EngineAPI.h"
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/include/Scripting/EngineAPI/NOUS_SCANCODE.h"
        DESTINATION
        "${SCRIPTS_SDK_INCLUDE_DIR}/Scripting/EngineAPI"
)

file(COPY
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/src/EngineAPI/EngineAPI.cpp"
        DESTINATION
        "${SCRIPTS_SDK_SRC_DIR}"
)

# All binding headers, flat -- the per-domain folders live under src/ now, so the
# old relative-path walk is gone.
file(GLOB SDK_BINDINGS_HEADERS
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/include/Scripting/EngineAPI/Bindings/*.h"
)

file(COPY ${SDK_BINDINGS_HEADERS}
        DESTINATION "${SCRIPTS_SDK_INCLUDE_DIR}/Scripting/EngineAPI/Bindings"
)

# Discover script sources from source Assets/Scripts — edits there take effect immediately
file(GLOB_RECURSE SCRIPT_SOURCES
        CONFIGURE_DEPENDS
        "${SCRIPTS_SRC_DIR}/*.cpp"
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/src/EngineAPI/EngineAPI.cpp"
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
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/include"
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