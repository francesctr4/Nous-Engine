#ifndef VULKANSHADER_H
#define VULKANSHADER_H

#include "Engine/Renderer/RendererTypes.h"
#include "Engine/Renderer/Backend/Vulkan/VulkanTypes.inl"

#include <vector>

class ResourceShader;

// ─────────────────────────────────────────────────────────────────────────────
// VulkanShader
//
// Concrete IBackendShader implementation. Owns all Vulkan resources for a
// single generic shader: shader modules, descriptor set layouts, and the
// graphics pipeline (layout + handle).
// ─────────────────────────────────────────────────────────────────────────────
struct VulkanShader : public IBackendShader
{
    std::vector<VulkanShaderStage>     stages;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts; // one per set index, ordered
    VulkanPipeline                     pipeline;
    VulkanContext*                     vkContext = nullptr;

    // IBackendShader
    void Bind()    override;
    void Destroy() override;
};

namespace NOUS_VulkanShader
{
    // Allocates a VulkanShader, builds all GPU resources from the ResourceShader's
    // SPIR-V binaries and reflection data, and stores the result in
    // shader->internalData. renderpass is the target renderpass for the pipeline.
    bool Create(VulkanContext* vkContext, VulkanRenderpass* renderpass, ResourceShader* shader);

    // Destroys all GPU resources held by vs and frees the struct.
    // The caller is responsible for nulling shader->internalData afterwards.
    void Destroy(VulkanContext* vkContext, VulkanShader* vs);
}

#endif // VULKANSHADER_H
