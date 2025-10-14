# ==============================
# Nous Engine - Scripting System
# ==============================

set(SCRIPTING_SOURCES
        ScriptManager.cpp
        ScriptManager.h

        Internal/IScript.inl
        Internal/ScriptTemplate.inl
        Internal/ScriptRegistry.inl

        EngineAPI/ScriptBindings.cpp
        EngineAPI/ScriptBindings.h
        EngineAPI/EngineAPI.cpp
        EngineAPI/Bindings/InputBindings.cpp
        EngineAPI/Bindings/InputBindings.h
        EngineAPI/Bindings/LoggerBindings.cpp
        EngineAPI/Bindings/LoggerBindings.h
        EngineAPI/Bindings/GameObjectBindings.cpp
        EngineAPI/Bindings/GameObjectBindings.h
)

add_library(Scripting OBJECT ${SCRIPTING_SOURCES})

# Include root source so "Scripting/..." includes work
target_include_directories(Scripting PUBLIC ../..)

find_package(glm REQUIRED)
target_link_libraries(Scripting PUBLIC glm::glm)

set_target_properties(Scripting PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)

add_subdirectory(Runtime "${CMAKE_CURRENT_BINARY_DIR}/Runtime")