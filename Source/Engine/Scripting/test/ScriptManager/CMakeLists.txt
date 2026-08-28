# ===========================================
# t_Scripting_ScriptManager - ScriptManager's own logic
#
# Separate target from the platform test above: that one deliberately links NO
# engine code (it tests the OS mechanism), this one links the class under test.
# ===========================================

set(TEST_NAME "t_Scripting_ScriptManager")

add_executable(${TEST_NAME}
        t_ScriptManager.cpp
)

target_link_libraries(${TEST_NAME}
        PRIVATE
        # Narrow link: the static libraries this test actually uses, not the
        # NousEngine::Engine umbrella. See EngineCore/EngineExport.h.
        Scripting
        MemoryManager
        GTest::gtest
        GTest::gtest_main
)

# Engine code is linked INTO this binary, so the API macros must be neither
# dllimport nor dllexport. See EngineCore/EngineExport.h.
target_compile_definitions(${TEST_NAME} PRIVATE NOUS_ENGINE_STATIC)

set_target_properties(${TEST_NAME} PROPERTIES
        CXX_STANDARD 23
        CXX_STANDARD_REQUIRED ON
        CXX_EXTENSIONS OFF
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME ${TEST_NAME}
        COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
