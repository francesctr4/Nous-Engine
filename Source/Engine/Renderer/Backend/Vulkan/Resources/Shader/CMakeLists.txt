target_sources(VulkanBackend PRIVATE
        VulkanShaderDescriptorState.h
        VulkanShader.h
        VulkanShader.cpp
)

if (NOUS_BUILD_TESTS)
    add_subdirectory(test)
endif()
