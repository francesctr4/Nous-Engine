# ===========================================
# ResourceShader - Unit Tests
# ===========================================

set(TEST_NAME "t_ResourceShader")

add_executable(${TEST_NAME}
        t_ImporterShader.cpp
        t_ResourceShader.cpp
)

# These must match the packages found in your main Nous Engine build
find_package(GTest CONFIG REQUIRED)

target_link_libraries(${TEST_NAME}
        PRIVATE
        NousEngine::Engine
        ResourceManager_private   # covers ImporterShader.h
        NousEngine::Editor
        GTest::gtest
        GTest::gtest_main
)

# Register test
include(GoogleTest)

set_target_properties(${TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)