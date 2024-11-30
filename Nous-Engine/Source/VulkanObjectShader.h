#pragma once

#include "RendererTypes.inl"
#include "VulkanTypes.inl"

bool CreateObjectShader(VulkanContext* vkContext, VulkanObjectShader* outShader);

void DestroyObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader);

void UseObjectShader(VulkanContext* vkContext, VulkanObjectShader* shader);

void UpdateObjectShaderGlobalState(VulkanContext* vkContext, VulkanObjectShader* shader, float deltaTime);

void UpdateObjectShaderLocalState(VulkanContext* vkContext, VulkanObjectShader* shader, GeometryRenderData renderData);

bool AcquireObjectShaderResources(VulkanContext* vkContext, VulkanObjectShader* shader, uint32* outObjectID);

void ReleaseObjectShaderResources(VulkanContext* vkContext, VulkanObjectShader* shader, uint32 objectID);