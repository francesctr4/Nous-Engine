#ifndef VULKANSHADER_H
#define VULKANSHADER_H

#include <Renderer/RendererTypes.h>
#include "VulkanTypes.inl"
#include "Resources/Shader/VulkanShaderDescriptorState.h"

#include <mutex>
#include <string>
#include <vector>

class ResourceShader;

// ── Constants ─────────────────────────────────────────────────────────────────
constexpr uint32_t VULKAN_SHADER_MAX_INSTANCE_COUNT = 1024;

// ── Per-instance (per-material / per-object) state ───────────────────────────
struct VulkanShaderInstanceState
{
    // One descriptor set per swapchain image; only the first swapChainImages.size()
    // are allocated/used. Value-init nulls every slot (VK_NULL_HANDLE == 0).
    std::array<VkDescriptorSet, MAX_SWAPCHAIN_IMAGES> descriptorSets{};

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
    VulkanPipeline                     noDepthPipeline;             // Same as `pipeline`, depth test + write OFF (opt-in)
    VulkanContext*                     vkContext = nullptr;

    // ── Global (set=0) resources ──────────────────────────────────────────────
    VkDescriptorPool             globalPool = VK_NULL_HANDLE;

    // Flat arrays of c_renderpassCount * globalImageCount entries, indexed through
    // GlobalSlot(vs, pass, image). Per PASS as well as per image because a user
    // shader is created ONCE but drawn in both the scene and game passes, which
    // carry different view matrices -- one buffer per image means the game pass
    // clobbers what the scene pass recorded. (The built-in material shader dodges
    // this by having a whole cloned ResourceShader per viewport; custom shaders
    // have no clone.)
    std::vector<VkDescriptorSet> globalDescriptorSets;
    std::vector<VulkanBuffer>    globalUBOBuffers;

    uint32_t                     globalUBOStride  = 0; // bytes for the set=0 UBO (from reflection)
    uint32_t                     globalImageCount = 0; // swapchain images this shader was built for

    // Last vkContext->globalUpdateStamp this shader's set=0 UBO was written for.
    // 0 means "never", and the context counter starts at 1 so the first draw always
    // updates.
    uint32_t                     lastGlobalStamp = 0;

    // True when set=0 declares the bone palette (binding 3). Derived once from
    // reflection at create time; a shader without it renders skinned meshes in bind
    // pose and warns once.
    bool                         supportsSkinning = false;

    // Set after that warning fires, so it is once per shader rather than once per
    // frame -- a 60 Hz log flood is the same as no warning.
    bool                         warnedMissingSkinning = false;

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

    // Build an ADDITIONAL depth-off pipeline (`noDepthPipeline`) alongside the
    // normal depth-tested one, so a single shader can draw some of its instances
    // occluded and others through geometry. Used by the wireframe debug family:
    // bounding boxes and light gizmos stay depth-tested while the skeleton draws
    // through the mesh. Differs from noDepthTest, which makes the ONLY pipeline
    // depth-off.
    bool createNoDepthVariant   = false;
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
     * @brief Bind the depth-off variant of the main pipeline, so these instances
     *        draw through geometry. Requires createNoDepthVariant; falls back to
     *        the depth-tested pipeline when the variant was not built, which
     *        degrades to an occluded overlay rather than an unbound pipeline.
     */
    void BindNoDepthPipeline(VkCommandBuffer cmdBuffer, VulkanShader* vs);

    /**
     * @brief Flat index into globalDescriptorSets / globalUBOBuffers.
     */
    [[nodiscard]] uint32_t GlobalSlot(const VulkanShader* vs, RenderpassType renderpassID,
                                      uint32_t imageIndex);

    /**
     * @brief Upload `data` (size bytes) to the global UBO for (renderpassID, imageIndex),
     *        update the descriptor, and bind descriptor set=0.
     */
    void UpdateGlobal(VulkanContext* vkContext, VkCommandBuffer cmdBuffer,
                      VulkanShader* vs, RenderpassType renderpassID, uint32_t imageIndex,
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
