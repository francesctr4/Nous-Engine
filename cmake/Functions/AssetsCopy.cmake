# ============================================
# AssetCopy.cmake — Copies project assets
# ============================================

function(copy_assets TARGET_NAME)
    if(EXISTS ${CMAKE_SOURCE_DIR}/Assets)
        add_custom_target(CopyAssets ALL
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${CMAKE_SOURCE_DIR}/Assets
                ${CMAKE_BINARY_DIR}/bin/Assets
                COMMENT "Copying Assets directory"
        )
        add_dependencies(${TARGET_NAME} CopyAssets)
    else()
        message(WARNING "Assets directory not found — skipping copy step.")
    endif()
endfunction()