# =========================================================
# InstallGame install component
#
# Produces a self-contained Delivery/Game/ folder with the
# minimum files needed to run the game. Only Library/ is required —
# no Assets/ or .meta files are shipped.
#
# Included from Source/Game/CMakeLists.txt after GameApp is defined.
#
# Usage in CLion: Build → InstallGame
# Manual:
#   cmake --install Build/Release-Windows --config Release \
#         --component InstallGame --prefix Delivery/Game
# =========================================================

add_custom_target(InstallGame
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CMAKE_SOURCE_DIR}/Delivery/Game"
        COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
                --config $<CONFIG>
                --component InstallGame
                --prefix "${CMAKE_SOURCE_DIR}/Delivery/Game"
        DEPENDS GameApp Nous-Engine Scripts
        COMMENT "Packaging InstallGame → Delivery/Game"
        VERBATIM
)

# GameApp.exe
install(TARGETS GameApp
        RUNTIME DESTINATION .
        COMPONENT InstallGame
)

# Nous-Engine.dll
install(TARGETS Nous-Engine
        RUNTIME DESTINATION .
        COMPONENT InstallGame
)

# Scripts.dll — loaded at runtime from the Scripts/ subdirectory
install(TARGETS Scripts
        RUNTIME DESTINATION Scripts
        COMPONENT InstallGame
)

# Runtime DLLs — scan PE import tables recursively to catch all transitive
# dependencies (SDL3, assimp and its sub-deps: kubazip, minizip, poly2tri,
# pugixml, zlib1, etc.) that TARGET_RUNTIME_DLLS misses.
# System DLLs (System32) are excluded.
install(CODE "
    file(GET_RUNTIME_DEPENDENCIES
        EXECUTABLES  \"$<TARGET_FILE:GameApp>\"
        DIRECTORIES  \"$<TARGET_FILE_DIR:GameApp>\"
        RESOLVED_DEPENDENCIES_VAR   _deps
        UNRESOLVED_DEPENDENCIES_VAR _unresolved
        PRE_EXCLUDE_REGEXES  \"api-ms-.*\" \"ext-ms-.*\"
        POST_EXCLUDE_REGEXES \".*[Ss]ystem32[/\\\\\\\\].*\\\\.dll\"
                             \"vulkan-1\\\\.dll\"
    )
    foreach(_dep IN LISTS _deps)
        file(INSTALL \"\${_dep}\" DESTINATION \"\${CMAKE_INSTALL_PREFIX}\")
    endforeach()
" COMPONENT InstallGame)

# Game configuration — start scene, target FPS, etc.
install(FILES "${CMAKE_SOURCE_DIR}/Source/Game/game_config.json"
        DESTINATION .
        COMPONENT InstallGame
)

# Compiled resource cache (SPIR-V shaders, meshes, textures, materials, scenes).
install(DIRECTORY "${CMAKE_SOURCE_DIR}/Library/"
        DESTINATION Library
        COMPONENT InstallGame
        PATTERN "_simulation_snapshot.nous" EXCLUDE
)

