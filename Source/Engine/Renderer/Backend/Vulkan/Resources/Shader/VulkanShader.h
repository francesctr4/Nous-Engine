#ifndef VULKANSHADER_H
#define VULKANSHADER_H

#include "Engine/Renderer/RendererTypes.h"
#include "Engine/Renderer/Backend/Vulkan/VulkanTypes.inl"

#include <string>
#include <vector>

class ResourceShader;

// ── Constants ─────────────────────────────────────────────────────────────────
constexpr uint32_t VULKAN_SHADER_MAX_INSTANCE_COUNT = 1024;

// ── Per-descriptor generation/ID tracking for lazy descriptor writes ──────────
struct VulkanShaderDescriptorState
{
    // One slot per swapchain image (triple-buffering)
    std::array<uint32_t, 3> generations = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
    std::array<uint32_t, 3> ids         = {UINT32_MAX, UINT32_MAX, UINT32_MAX};
};

// ── Per-instance (per-material / per-object) state ───────────────────────────
struct VulkanShaderInstanceState
{
    std::array<VkDescriptorSet, 3>           descriptorSets =
        {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};

    // One entry per binding in set=1; sized at allocation time.
    std::vector<VulkanShaderDescriptorState> descriptorStates;

    bool inUse = false;
};

// ── VulkanShader ──────────────────────────────────────────────────────────────
/**
 * @brief Concrete IBackendShader. Owns the Vulkan pipeline and all associated
 *        descriptor resources for one shader program.
 *
 * Descriptor convention (driven by reflection):
 *   set=0  — "global"   — one descriptor set per swapchain image (view/projection UBO)
 *   set=1  — "instance" — one descriptor set per acquired object (material UBO + samplers)
 */
struct VulkanShader : public IBackendShader
{
    // ── Pipeline ──────────────────────────────────────────────────────────────
    std::vector<VulkanShaderStage>     stages;
    std::vector<VkDescriptorSetLayout> descriptorSetLayouts; // indexed by set number
    VulkanPipeline                     pipeline;
    VulkanContext*                     vkContext = nullptr;

    // ── Global (set=0) resources ──────────────────────────────────────────────
    VkDescriptorPool             globalPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> globalDescriptorSets; // one per swapchain image
    std::vector<VulkanBuffer>    globalUBOBuffers;      // one per swapchain image
    uint32_t                     globalUBOStride = 0;  // bytes for the set=0 UBO (from reflection)

    // ── Instance (set=1) resources ────────────────────────────────────────────
    VkDescriptorPool             instancePool = VK_NULL_HANDLE;
    VulkanBuffer                 instanceUBOBuffer;      // shared; stride * MAX_INSTANCE_COUNT
    uint32_t                     instanceUBOStride = 0; // bytes per instance (aligned)
    uint32_t                     instanceBindingCount = 0; // bindings in set=1
    std::vector<VulkanShaderInstanceState> instanceStates; // VULKAN_SHADER_MAX_INSTANCE_COUNT

    // ── IBackendShader ────────────────────────────────────────────────────────
    void Bind()    override;
    void Destroy() override;
};

// ── NOUS_VulkanShader namespace ───────────────────────────────────────────────
namespace NOUS_VulkanShader
{
    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /**
     * @brief Build all Vulkan resources from a ResourceShader that already holds
     *        compiled SPIR-V and reflection data.  Populates shader->internalData.
     */
    bool Create(VulkanContext* vkContext, VulkanRenderpass* renderpass,
                ResourceShader* shader);

    /** @brief Destroy all GPU resources held by vs and free the struct. */
    void Destroy(VulkanContext* vkContext, VulkanShader* vs);

    // ── Per-frame operations ──────────────────────────────────────────────────

    /** @brief Bind the VkPipeline. */
    void BindPipeline(VkCommandBuffer cmdBuffer, VulkanShader* vs);

    /**
     * @brief Upload `data` (size bytes) to the global UBO for `imageIndex`,
     *        update the descriptor, and bind descriptor set=0.
     */
    void UpdateGlobal(VulkanContext* vkContext, VkCommandBuffer cmdBuffer,
                      VulkanShader* vs, uint32_t imageIndex,
                      const void* data, uint64_t size);

    // ── Instance slot management ──────────────────────────────────────────────

    /** @brief Acquire a free instance slot. Returns false if the pool is full. */
    bool AcquireInstanceSlot(VulkanContext* vkContext, VulkanShader* vs,
                              uint32_t* outID);

    /** @brief Release an instance slot back to the pool. */
    void ReleaseInstanceSlot(VulkanContext* vkContext, VulkanShader* vs,
                              uint32_t id);

    // ── Per-instance descriptor writes ───────────────────────────────────────

    /**
     * @brief Write UBO data to set=1, binding=0 for the given instance + image.
     *        Always uploads; skips descriptor write only when inOutGeneration is valid.
     */
    void WriteInstanceUBO(VulkanContext* vkContext, VulkanShader* vs,
                          uint32_t imageIndex, uint32_t instanceID,
                          const void* data, uint64_t size,
                          uint32_t* inOutGeneration);

    /**
     * @brief Write a combined image sampler to set=1 at `bindingIndex` for the
     *        given instance + image.  Lazily skips writes when resource unchanged.
     */
    void WriteInstanceSampler(VulkanContext* vkContext, VulkanShader* vs,
                               uint32_t imageIndex, uint32_t instanceID,
                               uint32_t bindingIndex,
                               VkImageView imageView, VkSampler sampler,
                               uint32_t* inOutGeneration, uint32_t* inOutID,
                               uint32_t resourceID, uint32_t resourceGeneration);

    /** @brief Bind the set=1 descriptor set for this instance + image index. */
    void BindInstanceDescriptorSet(VkCommandBuffer cmdBuffer, VulkanShader* vs,
                                   uint32_t imageIndex, uint32_t instanceID);
}

#endif // VULKANSHADER_H
