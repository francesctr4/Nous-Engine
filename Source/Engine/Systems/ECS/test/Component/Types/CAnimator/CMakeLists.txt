set(TEST_NAME "t_ECS_CAnimator")

add_executable(${TEST_NAME}
        t_CAnimator.cpp
)

find_package(GTest CONFIG REQUIRED)

# Links the static libraries it actually uses, NOT NousEngine::Engine.
#
# ResourceManager is linked for its OBJECTS, not just its headers: this test
# CONSTRUCTS ResourceSkeleton / ResourceAnimation on the stack and fills them by
# hand. That is the whole reason CAnimator is testable with no Application, no
# resource manager instance and no files on disk -- both types are plain
# ResourceBase subclasses holding public data. It also propagates
# ResourceManager_headers PUBLIC, which FakeComponentServices.h needs for
# IResourceLoader.
#
# AnimationSystem supplies AnimChannel / SkeletonData / Transform, built by hand
# here for the same reason.
target_link_libraries(${TEST_NAME}
        PRIVATE
        ECS
        AnimationSystem           # AnimChannel, SkeletonData, Transform
        ResourceManager           # ResourceSkeleton / ResourceAnimation ctors
        MemoryManager             # InitializeMemory / ShutdownMemory, called directly
        ECS_test_headers          # <FakeComponentServices.h>
        Scripting_headers         # IScriptRegistry, via FakeComponentServices.h
        GTest::gtest
        GTest::gtest_main
)

# Engine code is linked INTO this binary, so NOUS_ENGINE_API must be neither
# dllimport nor dllexport. See the comment in EngineCore/EngineExport.h.
target_compile_definitions(${TEST_NAME} PRIVATE NOUS_ENGINE_STATIC)

include(GoogleTest)

set_target_properties(${TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
