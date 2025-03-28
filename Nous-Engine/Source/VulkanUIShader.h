#pragma once

#include "RendererTypes.inl"
#include "VulkanTypes.inl"

class ResourceMaterial;

bool CreateUIShader(VulkanContext* vkContext, VulkanUIShader* outShader);
void DestroyUIShader(VulkanContext* vkContext, VulkanUIShader* shader);

void UseUIShader(VulkanContext* vkContext, VulkanUIShader* shader);

void UpdateUIShaderGlobalState(VulkanContext* vkContext, VulkanUIShader* shader, float deltaTime);

void UIShaderSetModel(VulkanContext* vkContext, VulkanUIShader* shader, float4x4 model);
void UIShaderApplyMaterial(VulkanContext* vkContext, VulkanUIShader* shader, ResourceMaterial* material);

bool AcquireUIShaderResources(VulkanContext* vkContext, VulkanUIShader* shader, ResourceMaterial* material);
void ReleaseUIShaderResources(VulkanContext* vkContext, VulkanUIShader* shader, ResourceMaterial* material);