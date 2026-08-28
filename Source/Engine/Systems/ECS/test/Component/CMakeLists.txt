add_subdirectory(Types)

find_package(GTest CONFIG REQUIRED)
find_package(parson CONFIG REQUIRED)

set(TEST_NAME "t_ComponentList")

add_executable(${TEST_NAME}
        ${TEST_NAME}.cpp
)

# Links the static libraries this test actually uses, not NousEngine::Engine.
# ECS carries most of the closure transitively (Utils, Logger, MemoryManager,
# NOUS_Multithreading, Renderer, and the AudioSystem/VideoSystem header dirs).
target_link_libraries(${TEST_NAME}
        PRIVATE
        ECS
        MemoryManager             # InitializeMemory / ShutdownMemory, called directly
        GTest::gtest
        GTest::gtest_main
        parson::parson
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
