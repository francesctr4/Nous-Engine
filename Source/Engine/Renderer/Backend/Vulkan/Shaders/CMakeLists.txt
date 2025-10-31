set(CLASS_NAME "VulkanShaderUtils")

target_sources(VulkanBackend PRIVATE
        ${CLASS_NAME}.h
        ${CLASS_NAME}.cpp
)

add_subdirectory(BuiltIn)