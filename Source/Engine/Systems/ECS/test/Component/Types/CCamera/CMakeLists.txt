set(TEST_NAME "t_ECS_CCamera")

add_executable(${TEST_NAME}
        t_CCamera.cpp
)

find_package(GTest CONFIG REQUIRED)

# Links the static libraries it actually uses, NOT NousEngine::Engine.
#
# It used to link the whole engine DLL plus NousEngine::Editor -- the editor for
# nothing at all, since this file names no editor type. That was invisible while
# the global Source/ include root was on every target's path: there was no reason
# to declare anything, so the umbrella was the path of least resistance. The cost
# is a relink of every test on any engine edit.
#
# ECS carries most of the closure transitively (Utils, Logger, MemoryManager,
# AudioSystem/VideoSystem headers, EngineCore). The last two entries do not: they
# are reached through FakeComponentServices.h, which ECS itself never includes.
target_link_libraries(${TEST_NAME}
        PRIVATE
        ECS
        MemoryManager             # InitializeMemory / ShutdownMemory, called directly
        ECS_test_headers          # <FakeComponentServices.h>
        ResourceManager_headers   # IResourceLoader, via FakeComponentServices.h
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
