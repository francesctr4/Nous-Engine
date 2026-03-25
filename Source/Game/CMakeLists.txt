# =========================================================
# Game Runtime Executable
# =========================================================

add_executable(GameApp
        MainGame.cpp
)

target_link_libraries(GameApp PRIVATE
        NousEngine::Engine
)

# Scripts.dll is loaded at runtime, not linked, but must be built first.
add_dependencies(GameApp Scripts)

set_target_properties(GameApp PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(GameApp PRIVATE
        ${CMAKE_SOURCE_DIR}/Source
)

include("${CMAKE_SOURCE_DIR}/cmake/Install/InstallGame.cmake")