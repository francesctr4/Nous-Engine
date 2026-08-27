# ==============================================================================
# Nous Engine - Scripting System
#
# Converted to the public/private header layout (see CONVENTIONS.md).
#
#   include/Scripting/**  is the API, reached as <Scripting/EngineAPI/EngineAPI.h>.
#   src/**                is implementation, reachable only from inside this target.
#
# THIS TARGET IS UNLIKE EVERY OTHER ONE: its public headers are also shipped as a
# user-facing SDK. Runtime/CMakeLists.txt copies include/Scripting/** verbatim into
#   <bin>/EngineCore/Scripts/SDK/include/Scripting/**
# and RebuildScripts.{bat,sh}.in re-syncs them on every hot-reload compile, so the
# include prefix below is what every user script in Assets/ says -- and what
# Internal/ScriptTemplate.inl generates into each NEW script. Changing it means
# changing those too. It was <Engine/Scripting/...> before the conversion; both
# were updated together, along with all eight Assets/Scripts/*.cpp.
#
# The payoff is that the SDK tree is now EXACTLY this target's public include tree,
# so a user script and an engine TU include the same path. Keep that property: if
# the two diverge, a script compiles in one place and fails in the other.
#
# Every header here is public -- the nine <Domain>Bindings.h look private by
# includer count (nothing outside names them) and are not: ScriptBindings.h
# includes all nine, and ScriptBindings.h is what user scripts include.
#
# ${CMAKE_SOURCE_DIR}/Source stays PUBLIC until the last target is converted.
# ==============================================================================

add_library(Scripting_headers INTERFACE)
target_include_directories(Scripting_headers INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# An _headers handle carries the header-level deps of the API it exposes:
# ScriptManager.h holds a NOUS_Vector, which routes through NOUS_STLAllocator.
target_link_libraries(Scripting_headers INTERFACE Utils_headers)

add_library(Scripting_private INTERFACE)
target_include_directories(Scripting_private INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_library(Scripting STATIC
        src/ScriptManager.cpp
)

target_sources(Scripting PRIVATE
        include/Scripting/ScriptManager.h
        include/Scripting/iScriptInput.h
        include/Scripting/iScriptRegistry.h
        include/Scripting/iScriptSceneHost.h
        include/Scripting/EngineAPI/EngineAPI.h
        include/Scripting/EngineAPI/NOUS_SCANCODE.h
        include/Scripting/EngineAPI/Bindings/ScriptBindings.h
        include/Scripting/EngineAPI/Bindings/CameraBindings.h
        include/Scripting/EngineAPI/Bindings/ComponentBindings.h
        include/Scripting/EngineAPI/Bindings/GameObjectBindings.h
        include/Scripting/EngineAPI/Bindings/InputBindings.h
        include/Scripting/EngineAPI/Bindings/LightBindings.h
        include/Scripting/EngineAPI/Bindings/LoggerBindings.h
        include/Scripting/EngineAPI/Bindings/MaterialBindings.h
        include/Scripting/EngineAPI/Bindings/SceneBindings.h
        include/Scripting/EngineAPI/Bindings/TimeBindings.h
        include/Scripting/Internal/IScript.inl
        include/Scripting/Internal/ScriptRegistry.inl
        include/Scripting/Internal/ScriptTemplate.inl
)

add_subdirectory(src/EngineAPI)
add_subdirectory(Runtime)

if(NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()

target_include_directories(Scripting
        PUBLIC  ${CMAKE_SOURCE_DIR}/Source          # temporary, see note above
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(Scripting PRIVATE NOUS_ENGINE_EXPORTS)

# Utils is PUBLIC and was previously undeclared entirely: ScriptManager.h holds a
# NOUS_Vector, which routes through NOUS_STLAllocator, so consumers of that header
# need Utils' API dir and MemoryManager's with it.
target_link_libraries(Scripting PUBLIC
        Scripting_headers
        glm::glm
        Utils
)

target_link_libraries(Scripting PRIVATE
        ECS
        Logger
        MemoryManager
        NOUS_Multithreading
        ResourceManager
        TimeManager
)

set_target_properties(Scripting PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
)
