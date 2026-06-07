set(TEST_NAME "t_AudioEffectRegistry")

add_executable(${TEST_NAME}
        ${TEST_NAME}.cpp
)

target_include_directories(${TEST_NAME}
        PRIVATE
        ${CMAKE_SOURCE_DIR}/Source
)

find_package(GTest CONFIG REQUIRED)

target_link_libraries(${TEST_NAME}
        PRIVATE
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

set(GAIN_TEST_NAME "t_GainDsp")

add_executable(${GAIN_TEST_NAME}
        ${GAIN_TEST_NAME}.cpp
)

target_include_directories(${GAIN_TEST_NAME}
        PRIVATE
        ${CMAKE_SOURCE_DIR}/Source
)

target_link_libraries(${GAIN_TEST_NAME}
        PRIVATE
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME ${GAIN_TEST_NAME} COMMAND ${GAIN_TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
