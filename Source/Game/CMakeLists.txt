# =========================================================
# Game Runtime Executable
# =========================================================

add_executable(GameApp
        MainGame.cpp
)

# ----------------------------------------
# Application icon (per-platform)
# ----------------------------------------
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/Branding/GameApp.ico")
        # The .rc compiles into the exe; rc resolves "GameApp.ico" relative to itself.
        target_sources(GameApp PRIVATE Branding/GameApp.rc)
# elseif(APPLE)
#         set(GAME_ICNS "${CMAKE_CURRENT_SOURCE_DIR}/Branding/GameApp.icns")
#         set_source_files_properties(${GAME_ICNS} PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
#         target_sources(GameApp PRIVATE ${GAME_ICNS})
#         set_target_properties(GameApp PROPERTIES
#                 MACOSX_BUNDLE TRUE
#                 MACOSX_BUNDLE_ICON_FILE "GameApp.icns")
# elseif(UNIX)
#         install(FILES Branding/GameApp.desktop DESTINATION share/applications)
#         install(FILES Branding/GameApp-256.png
#                 DESTINATION share/icons/hicolor/256x256/apps RENAME nous-game.png)
endif()

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

# Hide the console window in non-Debug builds (Release, Profiling, shipped).
# Debug keeps /SUBSYSTEM:CONSOLE so the output is visible when running from the IDE.
if(WIN32)
    target_link_options(GameApp PRIVATE
        $<$<NOT:$<CONFIG:Debug>>:/SUBSYSTEM:WINDOWS>
        $<$<NOT:$<CONFIG:Debug>>:/ENTRY:mainCRTStartup>
    )
endif()

target_include_directories(GameApp PRIVATE
        ${CMAKE_SOURCE_DIR}/Source
)

# Track game_config.json as an explicit dependency so it is copied to bin/Library/Settings/
# whenever it changes, even without a C++ recompile.  GameApp depends on the GameConfig target,
# ensuring the copy always runs before the executable is launched.
add_custom_command(
        OUTPUT  "${CMAKE_BINARY_DIR}/bin/Library/Settings/game_config.json"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_BINARY_DIR}/bin/Library/Settings"
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/game_config.json"
                "${CMAKE_BINARY_DIR}/bin/Library/Settings/game_config.json"
        DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/game_config.json"
        COMMENT "Copying game_config.json to bin/Library/Settings/"
)
add_custom_target(GameConfig DEPENDS "${CMAKE_BINARY_DIR}/bin/Library/Settings/game_config.json")
add_dependencies(GameApp GameConfig)

include("${CMAKE_SOURCE_DIR}/cmake/Install/InstallGame.cmake")