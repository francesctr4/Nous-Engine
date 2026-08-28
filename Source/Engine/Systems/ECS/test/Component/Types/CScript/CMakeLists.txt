# t_CScript — Awake/Start lifecycle across the two host orderings (editor vs game)
set(TEST_NAME "t_CScript")
add_executable(${TEST_NAME} ${TEST_NAME}.cpp)

find_package(GTest CONFIG REQUIRED)
# Scripting_headers: the test subclasses IScript and IScriptRegistry to observe the
# lifecycle. Headers only — no script DLL is involved.
target_link_libraries(${TEST_NAME} PRIVATE
        NousEngine::Engine
        NousEngine::Editor
        ECS_test_headers
        Scripting_headers
        GTest::gtest
        GTest::gtest_main
)
include(GoogleTest)
set_target_properties(${TEST_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME} WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
