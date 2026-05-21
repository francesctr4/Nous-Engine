# =========================================================
# InstallEngine install component
#
# Produces a self-contained Delivery/Nous-Engine/ folder with
# everything needed to run the editor.
#
# Must be included AFTER all subdirectories are added
# (Source/CMakeLists.txt) so that all targets are in scope.
#
# Usage in CLion: Build → InstallEngine
# Manual:
#   cmake --install Build/Release-Windows --config Release \
#         --component InstallEngine --prefix Delivery/Nous-Engine
# =========================================================

add_custom_target(InstallEngine
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CMAKE_SOURCE_DIR}/Delivery/Nous-Engine"
        COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
                --config $<CONFIG>
                --component InstallEngine
                --prefix "${CMAKE_SOURCE_DIR}/Delivery/Nous-Engine"
        DEPENDS EditorApp Nous-Editor Nous-Engine Scripts GameApp
        COMMENT "Packaging InstallEngine → Delivery/Nous-Engine"
        VERBATIM
)

# EditorApp.exe
install(TARGETS EditorApp
        RUNTIME DESTINATION .
        COMPONENT InstallEngine
)

# Nous-Engine.dll + Nous-Editor.dll
install(TARGETS Nous-Engine Nous-Editor
        RUNTIME DESTINATION .
        COMPONENT InstallEngine
)

# GameApp.exe — pre-built game stub used by the in-engine Build pipeline.
install(TARGETS GameApp
        RUNTIME DESTINATION Library/GameBin
        COMPONENT InstallEngine
)

# Scripts.dll → BUILD OUTPUT lives in Library/Scripts/ (user-deletable, regenerable)
install(TARGETS Scripts
        RUNTIME DESTINATION Library/Scripts
        COMPONENT InstallEngine
)

# Script build tooling — RebuildScripts.bat/sh, ScriptTemplate.inl, SDK/ headers+src.
# Installed to EngineCore/Scripts/ (NOT inside Library/) so it survives the user
# wiping Library/. Without this split, deleting Library/ takes the bat + SDK with
# it and the engine has no way to regenerate Scripts.dll on next launch.
install(DIRECTORY "${CMAKE_BINARY_DIR}/bin/EngineCore/Scripts/"
        DESTINATION EngineCore/Scripts
        COMPONENT InstallEngine
        PATTERN "*.pdb"    EXCLUDE   # Windows debug symbols
        PATTERN "*.exp"    EXCLUDE   # Windows export file
        PATTERN "*.lib"    EXCLUDE   # Windows import library
        PATTERN "*.rsp"    EXCLUDE   # compiler response file
        PATTERN "obj"      EXCLUDE
)

# Runtime DLLs — scan PE import tables recursively
install(CODE "
    file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES  \"$<TARGET_FILE:EditorApp>\"
        DIRECTORIES  \"$<TARGET_FILE_DIR:EditorApp>\"
        RESOLVED_DEPENDENCIES_VAR   _deps
        UNRESOLVED_DEPENDENCIES_VAR _unresolved
        PRE_EXCLUDE_REGEXES  \"api-ms-.*\" \"ext-ms-.*\"
        POST_EXCLUDE_REGEXES \".*[Ss]ystem32[/\\\\\\\\].*\\\\.dll\"
    )
    foreach(_dep IN LISTS _deps)
        file(INSTALL \"\${_dep}\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}\")
    endforeach()
" COMPONENT InstallEngine)

# Assets/ — full source assets (editor needs them for importing)
install(DIRECTORY "${CMAKE_SOURCE_DIR}/Assets/"
        DESTINATION Assets
        COMPONENT InstallEngine
)

# Game configuration — needed by the in-engine Build pipeline (copied into Library/Settings/).
install(FILES "${CMAKE_SOURCE_DIR}/Source/Game/game_config.json"
        DESTINATION Library/Settings
        COMPONENT InstallEngine
)

# Library/ — compiled resource cache
install(DIRECTORY "${CMAKE_SOURCE_DIR}/Library/"
        DESTINATION Library
        COMPONENT InstallEngine
        PATTERN "_simulation_snapshot.nous" EXCLUDE
)
