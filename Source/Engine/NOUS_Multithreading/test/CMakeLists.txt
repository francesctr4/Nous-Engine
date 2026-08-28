# Tests live at the target root, not under src/ — see CONVENTIONS.md.
# The sub-unit structure of src/ is mirrored here.
add_subdirectory(NOUS_Job)
add_subdirectory(NOUS_JobSystem)
add_subdirectory(NOUS_Thread)
add_subdirectory(NOUS_ThreadPool)

# ===========================================
# NOUS_Multithreading - Unit Tests
# ===========================================

set(TEST_NAME "t_NOUS_Multithreading")

add_executable(${TEST_NAME}
        t_NOUS_Multithreading.cpp
)

# These must match the packages found in your main Nous Engine build
find_package(GTest CONFIG REQUIRED)

target_link_libraries(${TEST_NAME}
        PRIVATE
        # Narrow link: the static libraries this test actually uses, not the
        # NousEngine::Engine umbrella. See EngineCore/EngineExport.h.
        NOUS_Multithreading
        MemoryManager
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

add_test(NAME ${TEST_NAME}
        COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
