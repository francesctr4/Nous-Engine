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

/**
 * @brief Checks the given expression's return value against VK_SUCCESS.
 */
#define VK_CHECK(expr)					\
{										\
	NOUS_ASSERT(expr == VK_SUCCESS);	\
}	

 /**
  * @brief Checks the given expression's return value against VK_SUCCESS.
  */
static void VK_CHECK_IMGUI(VkResult expr)
{
	if (expr != VK_SUCCESS) 
	{
		NOUS_FATAL(VkResultMessage(expr, true).c_str());
	}

	NOUS_ASSERT(expr == VK_SUCCESS);
}

 /**
  * @brief Checks the given expression's return value against VK_SUCCESS.
  */
#define VK_CHECK_MSG(expr, message)					\
{													\
	NOUS_ASSERT_MSG(expr == VK_SUCCESS, message);	\
}	