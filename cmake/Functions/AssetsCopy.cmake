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

    # Copy the entire Assets/ tree so user-created subfolders are staged automatically.
    file(COPY "${ASSETS_SRC_DIR}" DESTINATION "${CMAKE_BINARY_DIR}/bin")

    message(STATUS "Staged Assets -> ${ASSETS_BIN_DIR} (configure-time)")
endfunction()