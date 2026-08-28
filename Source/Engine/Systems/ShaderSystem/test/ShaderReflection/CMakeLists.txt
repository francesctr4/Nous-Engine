# ===========================================
# NOUS_Job - Unit Tests
# ===========================================

set(TEST_NAME "t_ShaderSystem_ShaderReflection")

add_executable(${TEST_NAME}
        t_ShaderReflection.cpp
)

# These must match the packages found in your main Nous Engine build
find_package(GTest CONFIG REQUIRED)

target_link_libraries(${TEST_NAME}
        PRIVATE
        ShaderSystem
        GTest::gtest
        GTest::gtest_main
)

# Register test
include(GoogleTest)

set_target_properties(${TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME ${TEST_NAME}
        COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ===========================================
# ShaderReflectionSerializer - Unit Tests
# ===========================================

# Distinct variable rather than reusing TEST_NAME: two targets in one
# CMakeLists that share a variable name are indistinguishable to tooling that
# reads these files (the test-target rename pass had to special-case exactly
# that in CVideoPlayer/CMakeLists.txt).
set(SERIALIZER_TEST_NAME "t_ShaderSystem_ShaderReflectionSerializer")

add_executable(${SERIALIZER_TEST_NAME}
        t_ShaderReflectionSerializer.cpp
)

# These must match the packages found in your main Nous Engine build
find_package(GTest CONFIG REQUIRED)

target_link_libraries(${SERIALIZER_TEST_NAME}
        PRIVATE
        ShaderSystem
        GTest::gtest
        GTest::gtest_main
)

# Register test
include(GoogleTest)

set_target_properties(${SERIALIZER_TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME ${SERIALIZER_TEST_NAME}
        COMMAND ${SERIALIZER_TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)