# These two tests cover deliberately dependency-free code, and their link lines
# say so: gtest plus an include-dir handle, no engine objects at all. Before the
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

# t_AudioEffectRegistry — covers a PUBLIC header, so it needs only the API dir.
set(TEST_NAME "t_AudioSystem_AudioEffectRegistry")

add_executable(${TEST_NAME} t_AudioEffectRegistry.cpp)

target_link_libraries(${TEST_NAME} PRIVATE
        AudioSystem_headers
        GTest::gtest
        GTest::gtest_main
)

set_target_properties(${TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# t_GainDsp — covers a PRIVATE header, so it opts in explicitly. This is the
# documented way to test implementation detail; nothing else may link _private.
set(GAIN_TEST_NAME "t_AudioSystem_GainDsp")

add_executable(${GAIN_TEST_NAME} t_GainDsp.cpp)

target_link_libraries(${GAIN_TEST_NAME} PRIVATE
        AudioSystem_private
        GTest::gtest
        GTest::gtest_main
)

set_target_properties(${GAIN_TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME ${GAIN_TEST_NAME} COMMAND ${GAIN_TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
