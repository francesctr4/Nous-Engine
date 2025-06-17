#pragma once

#include "RendererTypes.inl"
#include "VulkanTypes.inl"

class ResourceMaterial;

namespace NOUS_VulkanMaterialShader
{
	bool CreateMaterialShader(VulkanContext* vkContext, VulkanRenderpass* renderPass, VulkanMaterialShader* outShader);
	void DestroyMaterialShader(VulkanContext* vkContext, VulkanMaterialShader* shader);

	void UseMaterialShader(VulkanContext* vkContext, VulkanCommandBuffer* cmdBuffer, VulkanMaterialShader* shader);

	void UpdateMaterialShaderGlobalState(VulkanContext* vkContext, VulkanCommandBuffer* cmdBuffer, VulkanMaterialShader* shader, float deltaTime);

	void MaterialShaderSetModel(VulkanContext* vkContext, VulkanCommandBuffer* cmdBuffer, VulkanMaterialShader* shader, float4x4 model);
	void MaterialShaderApplyMaterial(VulkanContext* vkContext, VulkanCommandBuffer* cmdBuffer, VulkanMaterialShader* shader, ResourceMaterial* material);

	bool AcquireMaterialShaderResources(VulkanContext* vkContext, VulkanMaterialShader* shader, ResourceMaterial* material);
	void ReleaseMaterialShaderResources(VulkanContext* vkContext, VulkanMaterialShader* shader, ResourceMaterial* material);
}