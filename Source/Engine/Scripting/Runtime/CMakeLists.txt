# ==============================
# Nous Engine - Scripts DLL (Hot Reload)
# ==============================

set(SCRIPTS_OUTPUT_DIR "${CMAKE_BINARY_DIR}/bin/Scripts")

file(COPY "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/Runtime/RebuildScripts.bat" DESTINATION "${SCRIPTS_OUTPUT_DIR}")
file(COPY "${CMAKE_SOURCE_DIR}/Source/Engine/Scripting/Internal/ScriptTemplate.inl" DESTINATION "${SCRIPTS_OUTPUT_DIR}")

file(GLOB_RECURSE SCRIPT_SOURCES
        CONFIGURE_DEPENDS
        "${CMAKE_SOURCE_DIR}/Assets/Scripts/*.cpp"
        "../EngineAPI/EngineAPI.cpp"
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

# Include engine headers so scripts can use Engine API
target_include_directories(Scripts PUBLIC
        "${CMAKE_SOURCE_DIR}/Source/"
        "${CMAKE_SOURCE_DIR}/Assets/Scripts/"
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