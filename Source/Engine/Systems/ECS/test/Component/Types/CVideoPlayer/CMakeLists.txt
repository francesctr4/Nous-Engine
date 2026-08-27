# t_VideoPlayhead — pure-logic playhead test
set(TEST_NAME "t_VideoPlayhead")
add_executable(${TEST_NAME} ${TEST_NAME}.cpp)
target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Source)
find_package(GTest CONFIG REQUIRED)
# ECS_private: VideoPlayhead.h is implementation, and this is the documented
# opt-in for testing it. The test itself is pure -- no scene, no engine state.
target_link_libraries(${TEST_NAME} PRIVATE NousEngine::Engine NousEngine::Editor ECS_private GTest::gtest GTest::gtest_main)
include(GoogleTest)
set_target_properties(${TEST_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME} WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# t_CVideoPlayer — component defaults/serialization/ECS (added in Task 3)
set(TEST_NAME "t_CVideoPlayer")
add_executable(${TEST_NAME} ${TEST_NAME}.cpp)
target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/Source)
target_link_libraries(${TEST_NAME} PRIVATE NousEngine::Engine NousEngine::Editor ECS_test_headers GTest::gtest GTest::gtest_main)
set_target_properties(${TEST_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME} WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
