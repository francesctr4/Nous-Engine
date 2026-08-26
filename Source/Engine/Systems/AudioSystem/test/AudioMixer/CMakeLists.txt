# Covers a PRIVATE header (AudioBusMath is implementation detail), so it opts in
# through the _private handle. gtest + one include dir, no engine objects.
find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

set(TEST_NAME "t_AudioSystem_AudioBusMath")

add_executable(${TEST_NAME} t_AudioBusMath.cpp)

target_link_libraries(${TEST_NAME} PRIVATE
        AudioSystem_private
        GTest::gtest
        GTest::gtest_main
)

set_target_properties(${TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
