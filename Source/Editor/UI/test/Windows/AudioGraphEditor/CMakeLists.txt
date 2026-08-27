set(TEST_NAME "t_AudioGraphLinearize")

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
        # AudioGraphLinearize.h names AudioGraphDesc. Headers only -- this test
        # stays a pure headless test of the linearizer, with no engine objects.
        AudioSystem_headers
        # AudioGraphLinearize.h is a PUBLIC EditorUI header, so the API dir --
        # not EditorUI_private -- is what this test needs. Still no objects.
        EditorUI_headers
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
