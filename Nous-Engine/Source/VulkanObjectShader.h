#pragma once

#include "RendererTypes.inl"
#include "VulkanTypes.inl"

bool CreateObjectShader(VulkanContext* vkContext, VulkanObjectShader* outShader);

void DestroyObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader);

void UseObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader);

void UpdateGlobalStateObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader);

void UpdateObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader, float4x4 model);