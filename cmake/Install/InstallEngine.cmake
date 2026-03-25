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
        COMMAND "${CMAKE_COMMAND}" --install "${CMAKE_BINARY_DIR}"
                --config $<CONFIG>
                --component InstallEngine
                --prefix "${CMAKE_SOURCE_DIR}/Delivery/Engine"
        DEPENDS EditorApp Nous-Editor Nous-Engine Scripts
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

# Scripts.dll
install(TARGETS Scripts
        RUNTIME DESTINATION Scripts
        COMPONENT InstallEngine
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

# Library/ — compiled resource cache
install(DIRECTORY "${CMAKE_SOURCE_DIR}/Library/"
        DESTINATION Library
        COMPONENT InstallEngine
        PATTERN "_simulation_snapshot.nous" EXCLUDE
)
