# ===========================================
# t_FrameSyncPolicy - Unit Tests
# ===========================================

find_package(GTest CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
include(GoogleTest)

set(TEST_NAME "t_VulkanBackend_FrameSyncPolicy")

add_executable(${TEST_NAME}
        t_FrameSyncPolicy.cpp
)

# Links gtest, one include-dir handle and Vulkan headers -- NO engine target and
# no device. The header under test is pure frame-loop policy extracted out of
# VulkanBackend.cpp; see t_GainDsp and t_AudioBusMath for the same shape.
target_link_libraries(${TEST_NAME}
        PRIVATE
        VulkanBackend_private   # covers Core/FrameSyncPolicy.h
        Vulkan::Vulkan          # the policy is VkResult-typed
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
