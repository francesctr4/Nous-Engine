# ============================================
# Messages.cmake — Build summary
# ============================================

function(print_build_summary)
    message(STATUS "")
    message(STATUS "==============================")
    message(STATUS " Nous Engine Build Configuration")
    message(STATUS "==============================")
    message(STATUS " Engine DLL:        NousEngine.dll")
    message(STATUS " Editor Executable: EditorApp.exe")
    message(STATUS " C++ Standard:      ${CMAKE_CXX_STANDARD}")
    message(STATUS " Build Type:        ${CMAKE_BUILD_TYPE}")
    message(STATUS " Output Directory:  ${CMAKE_BINARY_DIR}/bin")
    message(STATUS "==============================")
    message(STATUS "")
endfunction()