#pragma once

#include "VulkanTypes.inl"

/**
 * Returns the string representation of result.
 * @param result The result to get the string for.
 * @param extended Indicates whether to also return an extended result.
 * @returns The error code and/or extended error message in string form. Defaults to success for unknown result types.
 */
std::string VkResultMessage(VkResult result, bool extended);

/**
 * Inticates if the passed result is a success or an error as defined by the Vulkan spec.
 * @returns True if success; otherwise false. Defaults to true for unknown result types.
 */
bool VkResultIsSuccess(VkResult result);