# ===========================================
# VideoFrameQueue - Unit Tests
# ===========================================

set(TEST_NAME "t_VideoFrameQueue")

add_executable(${TEST_NAME}
        t_VideoFrameQueue.cpp
)

target_include_directories(${TEST_NAME}
        PRIVATE
        ${CMAKE_SOURCE_DIR}/Source
)

find_package(GTest CONFIG REQUIRED)

target_link_libraries(${TEST_NAME}
        PRIVATE
        NousEngine::Engine
        VideoSystem_private   # covers a PRIVATE header — the documented opt-in
        NousEngine::Editor
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
