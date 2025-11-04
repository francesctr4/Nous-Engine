set(TEST_NAME "t_ModuleCamera3D")

add_executable(${TEST_NAME}
        ${TEST_NAME}.cpp
)

target_include_directories(${TEST_NAME}
        PRIVATE
        ../include
        ${CMAKE_SOURCE_DIR}/Source
)

# These must match the packages found in your main Nous Engine build
find_package(GTest CONFIG REQUIRED)

target_link_libraries(${TEST_NAME}
        PRIVATE
        NousEngine::Engine
        NousEngine::Editor
        GTest::gtest
        GTest::gtest_main
)

# Register test
include(GoogleTest)

set_target_properties(${TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

gtest_discover_tests(${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)