#pragma once

#include "RendererTypes.inl"
#include "VulkanTypes.inl"

bool CreateObjectShader(VulkanContext* vkContext, VulkanObjectShader* outShader);

void DestroyObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader);

void UseObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader);