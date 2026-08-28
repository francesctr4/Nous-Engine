# ===========================================
# t_GroupGeometries - Unit Tests
# ===========================================

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

set(TEST_NAME "t_RendererFrontend_GroupGeometries")

add_executable(${TEST_NAME}
        t_GroupGeometries.cpp
)

target_link_libraries(${TEST_NAME}
        PRIVATE
        # Narrow link: the static libraries this test actually uses, not the
        # NousEngine::Engine umbrella. See EngineCore/EngineExport.h.
        RendererFrontend
        GTest::gtest
        GTest::gtest_main
)

# Engine code is linked INTO this binary, so the API macros must be neither
# dllimport nor dllexport. See EngineCore/EngineExport.h.
target_compile_definitions(${TEST_NAME} PRIVATE NOUS_ENGINE_STATIC)

set_target_properties(${TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME ${TEST_NAME}
        COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
