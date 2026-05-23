# ===========================================
# NOUS_Job - Unit Tests
# ===========================================

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

# ── t_ShaderLoader ────────────────────────────────────────────────────────────
set(TEST_NAME "t_ShaderLoader")

add_executable(${TEST_NAME}
        ${TEST_NAME}.cpp
)

target_include_directories(${TEST_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_SOURCE_DIR}/Source
)

target_link_libraries(${TEST_NAME}
        PRIVATE
        ShaderSystem
        ResourceManager
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
set(HOT_RELOAD_TEST_NAME "t_ShaderHotReload")

add_executable(${HOT_RELOAD_TEST_NAME}
        ${HOT_RELOAD_TEST_NAME}.cpp
)

target_include_directories(${HOT_RELOAD_TEST_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../include
        ${CMAKE_SOURCE_DIR}/Source
)

target_link_libraries(${HOT_RELOAD_TEST_NAME}
        PRIVATE
        ShaderSystem
        ResourceManager
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