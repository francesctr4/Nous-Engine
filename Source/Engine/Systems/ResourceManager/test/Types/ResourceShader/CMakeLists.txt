# ===========================================
# ResourceShader - Unit Tests
# ===========================================

set(TEST_NAME "t_ResourceManager_ResourceShader")

add_executable(${TEST_NAME}
        t_ImporterShader.cpp
        t_ResourceShader.cpp
)

# These must match the packages found in your main Nous Engine build
find_package(GTest CONFIG REQUIRED)

target_link_libraries(${TEST_NAME}
        PRIVATE
        # Narrow link: the static libraries this test actually uses, not the
        # NousEngine::Engine umbrella. See EngineCore/EngineExport.h.
        ResourceManager
        ShaderSystem
        MemoryManager
        ResourceManager_private   # covers ImporterShader.h
        GTest::gtest
        GTest::gtest_main
)

# Engine code is linked INTO this binary, so the API macros must be neither
# dllimport nor dllexport. See EngineCore/EngineExport.h.
target_compile_definitions(${TEST_NAME} PRIVATE NOUS_ENGINE_STATIC)

# Register test
include(GoogleTest)

set_target_properties(${TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)