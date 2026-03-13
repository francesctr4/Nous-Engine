# ============================================
# AssetsCopy.cmake — Configure-time asset staging
# ============================================

function(copy_assets)
    set(ASSETS_SRC_DIR "${CMAKE_SOURCE_DIR}/Assets")
    set(ASSETS_BIN_DIR "${CMAKE_BINARY_DIR}/bin/Assets")

    if(NOT EXISTS "${ASSETS_SRC_DIR}")
        message(WARNING "Assets directory not found: ${ASSETS_SRC_DIR}")
        return()
    endif()

    # Ensure destination exists
    file(MAKE_DIRECTORY "${ASSETS_BIN_DIR}")

    set(_ASSET_SUBDIRS
            Fonts
            Materials
            Meshes
            Scenes
            Scripts
            Settings
            Shaders
            Textures
    )

    foreach(_dir IN LISTS _ASSET_SUBDIRS)
        if(EXISTS "${ASSETS_SRC_DIR}/${_dir}")
            file(COPY "${ASSETS_SRC_DIR}/${_dir}"
                    DESTINATION "${ASSETS_BIN_DIR}")
        endif()
    endforeach()

    message(STATUS "Staged Assets -> ${ASSETS_BIN_DIR} (configure-time)")
endfunction()