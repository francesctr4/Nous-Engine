find_package(GTest CONFIG REQUIRED)

# t_VideoPlayhead -- pure-logic playhead test.
# ECS_private: VideoPlayhead.h is implementation, and this is the documented
# opt-in for testing it. VideoPlayhead.cpp lives in ECS, so the objects come
# from there; nothing else in the engine is touched.
set(TEST_NAME "t_VideoPlayhead")

add_executable(${TEST_NAME}
        ${TEST_NAME}.cpp
)

target_link_libraries(${TEST_NAME}
        PRIVATE
        ECS
        ECS_private
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

# t_CVideoPlayer -- component defaults / serialization / ECS integration.
set(TEST_NAME "t_CVideoPlayer")

add_executable(${TEST_NAME}
        ${TEST_NAME}.cpp
)

# Links the static libraries this test actually uses, not NousEngine::Engine.
# ECS carries most of the closure transitively (Utils, Logger, MemoryManager,
# NOUS_Multithreading, Renderer, and the AudioSystem/VideoSystem header dirs).
# ResourceManager and Scripting do not arrive through ECS.
target_link_libraries(${TEST_NAME}
        PRIVATE
        ECS
        MemoryManager             # InitializeMemory / ShutdownMemory, called directly
        ResourceManager           # ResourceVideo, constructed directly
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
