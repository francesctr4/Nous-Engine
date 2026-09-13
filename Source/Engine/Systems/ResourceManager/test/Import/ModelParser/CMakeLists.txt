find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

# Both cover the assimp-free half of the model pre-pass, so neither needs assimp,
# a file on disk, or an Application. They link ResourceManager (for BuildSkeleton /
# BuildClip) plus AnimationSystem (SkeletonData / AnimClipData are its types).
foreach (PARSER_TEST SkeletonBuild ClipBuild)
    set(TEST_NAME "t_ResourceManager_${PARSER_TEST}")

    add_executable(${TEST_NAME} t_${PARSER_TEST}.cpp)

    set_target_properties(${TEST_NAME} PROPERTIES
            CXX_STANDARD 23
            CXX_STANDARD_REQUIRED ON
            CXX_EXTENSIONS OFF
            VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )

    target_compile_definitions(${TEST_NAME} PRIVATE NOUS_ENGINE_STATIC)

    target_link_libraries(${TEST_NAME} PRIVATE
            ResourceManager
            AnimationSystem
            GTest::gtest
            GTest::gtest_main
    )

    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}
            WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    )
endforeach ()
