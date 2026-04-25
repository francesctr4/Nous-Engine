# =========================================================
# InstallEngine install component
#
# Produces a self-contained Delivery/Engine/ folder with
# everything needed to run the editor.
#
# Must be included AFTER all subdirectories are added
# (Source/CMakeLists.txt) so that all targets are in scope.
#
# Usage in CLion: Build → InstallEngine
# Manual:
#   cmake --install Build/Release-Windows --config Release \
#         --component InstallEngine --prefix Delivery/Engine
# =========================================================

add_custom_target(InstallEngine
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CMAKE_SOURCE_DIR}/Delivery/Engine"
        COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
                --config $<CONFIG>
                --component InstallEngine
                --prefix "${CMAKE_SOURCE_DIR}/Delivery/Engine"
        DEPENDS EditorApp Nous-Editor Nous-Engine Scripts GameApp
        COMMENT "Packaging InstallEngine → Delivery/Engine"
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

# Scripts.dll
install(TARGETS Scripts
        RUNTIME DESTINATION Library/Scripts
        COMPONENT InstallEngine
)

# Script build tools — RebuildScripts.bat/sh, ScriptTemplate.inl, SDK/ headers+src.
# Excludes the Scripts shared library (already installed via install(TARGETS Scripts ...))
# and build artifacts that are only needed during compilation.
install(DIRECTORY "${CMAKE_BINARY_DIR}/bin/Library/Scripts/"
        DESTINATION Library/Scripts
        COMPONENT InstallEngine
        PATTERN "*.dll"    EXCLUDE   # Windows shared library
        PATTERN "*.pdb"    EXCLUDE   # Windows debug symbols
        PATTERN "*.exp"    EXCLUDE   # Windows export file
        PATTERN "*.lib"    EXCLUDE   # Windows import library
        PATTERN "*.so"     EXCLUDE   # Linux shared library
        PATTERN "*.so.*"   EXCLUDE   # Linux versioned shared library (e.g. libFoo.so.1.0)
        PATTERN "*.dylib"  EXCLUDE   # macOS shared library
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
