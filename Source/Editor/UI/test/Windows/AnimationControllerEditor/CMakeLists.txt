set(TEST_NAME "t_EditorUI_AnimationControllerBuild")

add_executable(${TEST_NAME}
        t_AnimationControllerBuild.cpp
)

find_package(GTest CONFIG REQUIRED)

target_link_libraries(${TEST_NAME}
        PRIVATE
        # AnimationControllerBuild.h names ControllerGraph and friends. Headers only:
        # the header is fully inline and calls nothing out of Controller.cpp, so this
        # stays a pure headless test of the build and validation rules, with no engine
        # objects, no editor and no ImGui.
        AnimationSystem_headers
        # AnimationControllerBuild.h is a PUBLIC EditorUI header, so the API dir --
        # not EditorUI_private -- is what this test needs. Still no objects.
        EditorUI_headers
        GTest::gtest
        GTest::gtest_main
)

add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
