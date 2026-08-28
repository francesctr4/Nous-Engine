set(TEST_NAME "t_ModuleRenderer3D_RenderPacketPolicy")

add_executable(${TEST_NAME}
        t_RenderPacketPolicy.cpp
)

find_package(GTest CONFIG REQUIRED)

# Links gtest, one include-dir handle and glm -- NO engine target. The header
# under test is the pure decision half of BuildRenderPacket, so the test needs
# no registry, camera or device. Same shape as t_AudioSystem_GainDsp and
# t_VulkanBackend_FrameSyncPolicy.
target_link_libraries(${TEST_NAME}
        PRIVATE
        ModuleRenderer3D_private   # covers RenderPacketPolicy.h
        glm::glm
        GTest::gtest
        GTest::gtest_main
)

include(GoogleTest)

set_target_properties(${TEST_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME}
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
