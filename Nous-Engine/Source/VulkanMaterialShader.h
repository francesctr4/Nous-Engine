#pragma once

#include "RendererTypes.inl"
#include "VulkanTypes.inl"

class ResourceMaterial;

bool CreateMaterialShader(VulkanContext* vkContext, VulkanMaterialShader* outShader);
void DestroyMaterialShader(VulkanContext* vkContext, VulkanMaterialShader* shader);

void UseMaterialShader(VulkanContext* vkContext, VulkanMaterialShader* shader);

void UpdateMaterialShaderGlobalState(VulkanContext* vkContext, VulkanMaterialShader* shader, float deltaTime);

void MaterialShaderSetModel(VulkanContext* vkContext, VulkanMaterialShader* shader, float4x4 model);
void MaterialShaderApplyMaterial(VulkanContext* vkContext, VulkanMaterialShader* shader, ResourceMaterial* material);

bool AcquireMaterialShaderResources(VulkanContext* vkContext, VulkanMaterialShader* shader, ResourceMaterial* material);
void ReleaseMaterialShaderResources(VulkanContext* vkContext, VulkanMaterialShader* shader, ResourceMaterial* material);