# =========================================================
# Game Runtime Executable
# =========================================================

add_executable(GameApp
        MainGame.cpp
)

find_package(parson CONFIG REQUIRED)

target_link_libraries(GameApp PRIVATE
        NousEngine::Engine
        parson::parson
)

# Scripts.dll is loaded at runtime, not linked, but must be built first.
add_dependencies(GameApp Scripts)

set_target_properties(GameApp PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(GameApp PRIVATE
        ${CMAKE_SOURCE_DIR}/Source
)

# Track game_config.json as an explicit dependency so it is copied to bin/ whenever it changes,
# even without a C++ recompile.  GameApp depends on the GameConfig target, ensuring the copy
# always runs before the executable is launched.
add_custom_command(
        OUTPUT  "${CMAKE_BINARY_DIR}/bin/game_config.json"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/game_config.json"
                "${CMAKE_BINARY_DIR}/bin/game_config.json"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/game_config.json"
        COMMENT "Copying game_config.json to bin/"
)
add_custom_target(GameConfig DEPENDS "${CMAKE_BINARY_DIR}/bin/game_config.json")
add_dependencies(GameApp GameConfig)

include("${CMAKE_SOURCE_DIR}/cmake/Install/InstallGame.cmake")