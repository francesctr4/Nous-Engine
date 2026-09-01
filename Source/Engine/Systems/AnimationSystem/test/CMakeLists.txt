find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

# These four cover a library with no engine dependencies, and their link lines say
# so: the AnimationSystem archive, gtest, and nothing else. No NousEngine::Engine,
# no NousEngine::Editor, no window, no device -- which is the whole reason the
# spec's build order puts the maths first. If one of these ever needs another
# engine target, the code under test has drifted out of the pure layer.
#
# NOUS_ENGINE_STATIC is defined even though nothing here is marked
# NOUS_ENGINE_API today: the archive is built with NOUS_ENGINE_EXPORTS, and
# without the STATIC state a test TU would emit __imp_-prefixed references the
# linker will not pull an archive member in to satisfy. See the three-state macro
# note in .claude/CLAUDE.md -- LNK4217 is that failure in its warning form.
foreach (ANIM_TEST Interpolate Sampling Blending Palette)
    set(TEST_NAME "t_AnimationSystem_${ANIM_TEST}")

    add_executable(${TEST_NAME} t_${ANIM_TEST}.cpp)

    set_target_properties(${TEST_NAME} PROPERTIES
            CXX_STANDARD 23
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    target_compile_definitions(${TEST_NAME} PRIVATE NOUS_ENGINE_STATIC)

    target_link_libraries(${TEST_NAME} PRIVATE
            AnimationSystem
            GTest::gtest
            GTest::gtest_main
    )

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endforeach ()
