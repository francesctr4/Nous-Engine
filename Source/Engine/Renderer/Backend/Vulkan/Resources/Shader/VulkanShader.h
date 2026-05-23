#ifndef VULKANSHADER_H
#define VULKANSHADER_H

#include "Engine/Renderer/RendererTypes.h"
#include "Engine/Renderer/Backend/Vulkan/VulkanTypes.inl"
#include "Engine/Renderer/Backend/Vulkan/Resources/Shader/VulkanShaderDescriptorState.h"

#include <mutex>
#include <string>
#include <vector>

class ResourceShader;

// ── Constants ─────────────────────────────────────────────────────────────────
constexpr uint32_t VULKAN_SHADER_MAX_INSTANCE_COUNT = 1024;

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
    VulkanPipeline                     pipeline;                    // Main / outline-draw pipeline (depth-aware)
    VulkanPipeline                     stencilWritePipeline;        // Stencil-write pass — depth-aware  (outline only)
    VulkanPipeline                     outlineNoDepthPipeline;      // Outline-draw pass  — depth OFF    (outline only)
    VulkanPipeline                     stencilWriteNoDepthPipeline; // Stencil-write pass — depth OFF    (outline only)
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
    mutable std::mutex                     instanceMutex;  // guards instanceStates slot acquisition/release

    // ── IBackendShader ────────────────────────────────────────────────────────
    void Bind()    override;
    void Destroy() override;
};

// ── Pipeline creation settings ────────────────────────────────────────────────
struct VulkanShaderCreateInfo
{
    bool disableBlending        = false; // disable alpha blending (e.g. pick shader)
    bool createOutlinePipelines = false; // also build stencil-write / outline pipeline variants
    bool useLineTopology        = false; // LINE_LIST topology instead of TRIANGLE_LIST
    bool noDepthTest            = false; // depth test + depth write both OFF (e.g. background)
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
                ResourceShader* shader,
                const VulkanShaderCreateInfo& settings = {});

    /** @brief Destroy all GPU resources held by vs and free the struct. */
    void Destroy(VulkanContext* vkContext, VulkanShader* vs);

    // ── Per-frame operations ──────────────────────────────────────────────────

    /** @brief Bind the main VkPipeline (outline-draw or regular). */
    void BindPipeline(VkCommandBuffer cmdBuffer, VulkanShader* vs);

    /** @brief Bind the stencil-write pipeline — depth-aware (outline shaders only). */
    void BindStencilWritePipeline(VkCommandBuffer cmdBuffer, VulkanShader* vs);

    /** @brief Bind the outline-draw pipeline — depth OFF (outline shaders only). */
    void BindOutlineNoDepthPipeline(VkCommandBuffer cmdBuffer, VulkanShader* vs);

    /** @brief Bind the stencil-write pipeline — depth OFF (outline shaders only). */
    void BindStencilWriteNoDepthPipeline(VkCommandBuffer cmdBuffer, VulkanShader* vs);

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

    /** @brief Bind the set=1 descriptor set for this instance + image index.
     *  @param overrideLayout  If not VK_NULL_HANDLE, use this pipeline layout for the bind
     *                         instead of vs->pipeline.pipelineLayout. Pass vsDraw's layout
     *                         when the active pipeline was created from a different shader. */
    void BindInstanceDescriptorSet(VkCommandBuffer cmdBuffer, VulkanShader* vs,
                                   uint32_t imageIndex, uint32_t instanceID,
                                   VkPipelineLayout overrideLayout = VK_NULL_HANDLE);
}

#endif // VULKANSHADER_H
