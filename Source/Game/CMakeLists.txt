# =========================================================
# Game Runtime Executable
# =========================================================
add_executable(GameApp
        MainGame.cpp
)

target_link_libraries(GameApp PRIVATE
        NousEngine::Engine
)

# Output to the same bin folder for consistency
set_target_properties(GameApp PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

target_include_directories(GameApp PUBLIC ../)