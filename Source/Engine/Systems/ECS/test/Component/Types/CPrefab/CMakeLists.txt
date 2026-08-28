set(TEST_NAME "t_CPrefab")

add_executable(${TEST_NAME}
        ${TEST_NAME}.cpp
)

find_package(GTest CONFIG REQUIRED)

target_link_libraries(${TEST_NAME}
        PRIVATE
        NousEngine::Engine
        NousEngine::Editor
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)

set_target_properties(${TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
