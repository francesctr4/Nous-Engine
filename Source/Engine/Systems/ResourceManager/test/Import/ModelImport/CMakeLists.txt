find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

# Pure: no assimp, no file on disk, no Application. PlanModelAssets decides what
# sibling assets a parsed model implies and what they are called, from a hand-built
# ModelImportData -- the same split SkeletonBuild and ClipBuild draw, so the half
# with the filename traps and the clip-index off-by-one is reachable without an FBX.
#
# Links ResourceManager (for PlanModelAssets) plus AnimationSystem (SkeletonData and
# AnimClipData are its types, held by value in ModelImportData).
set(TEST_NAME "t_ResourceManager_ModelAssetPlan")

add_executable(${TEST_NAME} t_ModelAssetPlan.cpp)

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
