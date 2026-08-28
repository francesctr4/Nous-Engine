# ===========================================
# t_VulkanShaderDescriptorState - Unit Tests
# ===========================================

find_package(GTest CONFIG REQUIRED)
include(GoogleTest)

set(TEST_NAME "t_VulkanBackend_VulkanShaderDescriptorState")

add_executable(${TEST_NAME}
        t_VulkanShaderDescriptorState.cpp
)

# Links gtest plus one include-dir handle and NO engine target: the header under
# test is dependency-free policy (a generation/id pair per binding). This is the
# shape every unit test should aim for -- see t_GainDsp and t_AudioBusMath.
target_link_libraries(${TEST_NAME}
        PRIVATE
        VulkanBackend_private   # covers VulkanShaderDescriptorState.h
        GTest::gtest
        GTest::gtest_main
)

set_target_properties(${TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME ${TEST_NAME}
        COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
