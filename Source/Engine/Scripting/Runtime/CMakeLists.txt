# ==============================
# Nous Engine - Scripts DLL (Hot Reload)
# ==============================

set(SCRIPTS_OUTPUT_DIR     "${CMAKE_BINARY_DIR}/bin/Scripts")
set(SCRIPTS_ASSETS_BIN_DIR "${CMAKE_BINARY_DIR}/bin/Assets/Scripts")

# Tools/templates copied next to the scripts output
file(MAKE_DIRECTORY "${SCRIPTS_OUTPUT_DIR}")
file(COPY "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/Runtime/RebuildScripts.bat"
        DESTINATION "${SCRIPTS_OUTPUT_DIR}")
file(COPY "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/Internal/ScriptTemplate.inl"
        DESTINATION "${SCRIPTS_OUTPUT_DIR}")

set(SCRIPTS_SDK_DIR "${SCRIPTS_OUTPUT_DIR}/SDK")
set(SCRIPTS_SDK_INCLUDE_DIR "${SCRIPTS_SDK_DIR}/include")

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
        DESTINATION
        "${SCRIPTS_SDK_INCLUDE_DIR}/Engine/Scripting/EngineAPI"
)

file(COPY
        "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/EngineAPI/EngineAPI.cpp"
        DESTINATION
        "${SCRIPTS_SDK_INCLUDE_DIR}/Engine/Scripting/EngineAPI"
)

# Ensure bin/Assets/Scripts exists (it should, if you stage assets on configure)
if(NOT EXISTS "${SCRIPTS_ASSETS_BIN_DIR}")
    message(WARNING "Binary scripts assets directory not found: ${SCRIPTS_ASSETS_BIN_DIR}")
    message(WARNING "Make sure your configure-time asset replication stages Assets/Scripts into bin/Assets/Scripts.")
endif()

# Discover script sources ONLY from binary assets path
file(GLOB_RECURSE SCRIPT_SOURCES
        CONFIGURE_DEPENDS
        "${SCRIPTS_ASSETS_BIN_DIR}/*.cpp"
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

# Include engine headers + binary scripts include dir
target_include_directories(Scripts PUBLIC
        "${CMAKE_SOURCE_DIR}/Source/"
        "${SCRIPTS_ASSETS_BIN_DIR}"
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