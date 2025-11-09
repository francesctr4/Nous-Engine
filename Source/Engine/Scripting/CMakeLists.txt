# ==============================
# Nous Engine - Scripting System
# ==============================

add_library(Scripting STATIC
        ScriptManager.h
        ScriptManager.cpp
)

add_subdirectory(EngineAPI)
add_subdirectory(Internal)
add_subdirectory(Runtime)

target_include_directories(Scripting PUBLIC
        ${CMAKE_SOURCE_DIR}/Source
)

target_compile_definitions(Scripting PRIVATE NOUS_ENGINE_EXPORTS)

target_link_libraries(Scripting PUBLIC glm::glm)

set_target_properties(Scripting PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)