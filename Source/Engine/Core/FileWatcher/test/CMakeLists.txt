set(TEST_NAME "t_FileWatcher")

add_executable(${TEST_NAME}
        ${TEST_NAME}.cpp
)

find_package(GTest CONFIG REQUIRED)

target_include_directories(${TEST_NAME}
        PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/Source
)

target_link_libraries(${TEST_NAME}
        PRIVATE
        NousEngine::Engine
        GTest::gtest
        GTest::gtest_main
)

set_target_properties(${TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

include(GoogleTest)
add_test(NAME ${TEST_NAME}
        COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
