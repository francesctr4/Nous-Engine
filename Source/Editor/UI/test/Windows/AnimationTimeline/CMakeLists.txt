set(TEST_NAME "t_EditorUI_AnimationTimelineLayout")

add_executable(${TEST_NAME}
        t_AnimationTimelineLayout.cpp
)

find_package(GTest CONFIG REQUIRED)

target_link_libraries(${TEST_NAME}
        PRIVATE
        # AnimationTimelineLayout.h is fully inline and names no engine type, no ImGui
        # and no runtime -- the AudioGraphLinearize shape. So this is the API include
        # dir and gtest, with no engine objects at all.
        EditorUI_headers
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
