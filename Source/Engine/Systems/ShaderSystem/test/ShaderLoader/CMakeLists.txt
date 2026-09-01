# ===========================================
# NOUS_Job - Unit Tests
# ===========================================

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

# ── t_ShaderLoader ────────────────────────────────────────────────────────────
set(TEST_NAME "t_ShaderSystem_ShaderLoader")

add_executable(${TEST_NAME}
        t_ShaderLoader.cpp
)

target_link_libraries(${TEST_NAME}
        PRIVATE
        ShaderSystem
        MemoryManager
        FileSystem
        Logger
        TimeManager
        GTest::gtest
        GTest::gtest_main
)

set_target_properties(${TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME ${TEST_NAME}
        COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ── t_ShaderHotReload ─────────────────────────────────────────────────────────
set(HOT_RELOAD_TEST_NAME "t_ShaderSystem_ShaderHotReload")

add_executable(${HOT_RELOAD_TEST_NAME}
        t_ShaderHotReload.cpp
)

target_link_libraries(${HOT_RELOAD_TEST_NAME}
        PRIVATE
        ShaderSystem
        MemoryManager
        FileSystem
        Logger
        TimeManager
        GTest::gtest
        GTest::gtest_main
)

set_target_properties(${HOT_RELOAD_TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME ${HOT_RELOAD_TEST_NAME}
        COMMAND ${HOT_RELOAD_TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)