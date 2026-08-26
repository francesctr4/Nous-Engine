#pragma once

#include "Engine/Core/Globals.h"

#include <vulkan/vulkan.h>

#include <vector>

// -----------------------------------------------------------------------------
// The renderer, as seen from inside the editor's ImGui layer.
// -----------------------------------------------------------------------------
/**
 * @brief The narrow set of GPU handles and operations the editor legitimately needs.
 *
 * Implemented by VulkanBackend and injected into ModuleEditor, which hands it to
 * the editor windows through EditorContext. It replaces
 * VulkanBackend::GetVulkanContext() -- a static accessor that handed out the whole
 * mutable VulkanContext to anything that included the backend header.
 *
 * Three defects it closes:
 *  1. Ambient access. A static accessor is callable from anywhere at any time and
 *     is one-per-process; this pointer only reaches code that was given it.
 *  2. Over-exposure. ~10 fields are actually needed; the whole mutable context
 *     travelled.
 *  3. Uncontrolled mutation. The editor wrote directly into renderer-owned state
 *     (imGuiResources.m_*DescriptorSets). Those writes now go through
 *     SetViewportDescriptorSets / TakeViewportDescriptorSets.
 *
 * Vulkan-typed on purpose. The editor already includes vulkan.h for
 * ImGui_ImplVulkan (~9 raw handles are irreducibly required by
 * ImGui_ImplVulkan_Init), so "the editor must never see Vulkan" is not the goal
 * and the types cost nothing new. What the interface buys is that the editor no
 * longer sees VulkanContext.
 *
 * Note the direction: this header lives in Renderer/ (the layer being depended
 * upon by the editor) and is implemented by the backend -- the mirror image of
 * IRenderWindow / IRenderResourceProvider, which live here and are implemented
 * by Modules/.
 */

/** @brief Which offscreen viewport a call refers to. */
enum class EditorViewport : uint8
{
    Scene,
    Game
};

/**
 * @brief The error reporter ImGui calls on a failed VkResult.
 *
 * Supplied by the backend rather than written editor-side: reporting a VkResult
 * means turning it into a message, which is renderer-owned knowledge. Carrying it
 * here is what lets the editor drop its last include of a renderer-internal
 * Vulkan header.
 */
using PfnEditorVkCheckResult = void (*)(VkResult);

/** @brief Exactly what ImGui_ImplVulkan_Init requires. */
struct EditorGpuInfo
{
    VkInstance             instance            = VK_NULL_HANDLE;
    VkPhysicalDevice       physicalDevice      = VK_NULL_HANDLE;
    VkDevice               device              = VK_NULL_HANDLE;
    uint32                 graphicsQueueFamily = 0;
    VkQueue                graphicsQueue       = VK_NULL_HANDLE;
    VkDescriptorPool       descriptorPool      = VK_NULL_HANDLE;
    VkRenderPass           uiRenderpass        = VK_NULL_HANDLE;
    VkAllocationCallbacks* allocator           = nullptr;
    uint32                 imageCount          = 0;
    PfnEditorVkCheckResult checkVkResultFn     = nullptr;
};

/** @brief The offscreen colour targets an editor viewport samples from. */
struct EditorViewportImages
{
    std::vector<VkImageView> views;
    VkSampler                sampler = VK_NULL_HANDLE;
};

class IEditorRenderBridge
{
public:
    virtual ~IEditorRenderBridge() = default;

    // ───────────────────────────── ImGui backend lifecycle ───────────────────

    /** @brief The handles needed to build an ImGui_ImplVulkan_InitInfo. */
    [[nodiscard]] virtual EditorGpuInfo GetGpuInfo() const = 0;

    /** @brief Release the renderer-owned ImGui resource block (descriptor pool,
     *         viewport images, samplers). Call after ImGui_ImplVulkan_Shutdown. */
    virtual void DestroyImGuiResources() = 0;

    /** @brief Rebuild the ImGui resource block after a swapchain resize. The
     *         caller must have destroyed its viewport descriptor sets first and
     *         must recreate them afterwards -- the image views change. */
    virtual void RecreateImGuiResources() = 0;

    /** @brief The command buffer the UI renderpass is recording into this frame.
     *         Folds in the current swapchain image index. */
    [[nodiscard]] virtual VkCommandBuffer GetCurrentUICommandBuffer() const = 0;

    // ───────────────────────────── Viewport textures ─────────────────────────

    /** @brief ImTextureID for this frame's image of the given viewport. */
    [[nodiscard]] virtual uint64 GetViewportTexture(EditorViewport viewport) const = 0;

    /** @brief The image views + sampler to build descriptor sets against.
     *         One view per swapchain image. */
    [[nodiscard]] virtual EditorViewportImages GetViewportImages(EditorViewport viewport) const = 0;

    /**
     * @brief Hand ownership of freshly built descriptor sets back to the renderer.
     *
     * ImGui_ImplVulkan_AddTexture must be called editor-side (it is an ImGui
     * function, and the renderer does not link ImGui), but the storage is
     * renderer state -- hence the split.
     */
    virtual void SetViewportDescriptorSets(EditorViewport viewport,
                                           std::vector<VkDescriptorSet> descriptorSets) = 0;

    /**
     * @brief Return the viewport's current descriptor sets and clear the
     *        renderer-side storage in one step.
     *
     * Returning-and-clearing is deliberate: the caller must call
     * ImGui_ImplVulkan_RemoveTexture on exactly the sets that exist, which can be
     * fewer than the image count before Create has run. Handing back the vector
     * itself makes "iterate the descriptor-set vector's own size" true by
     * construction rather than by convention (an array<3> once masked this and
     * caused a resize-time OOB crash).
     */
    [[nodiscard]] virtual std::vector<VkDescriptorSet> TakeViewportDescriptorSets(EditorViewport viewport) = 0;

    // ───────────────────────────── Misc ──────────────────────────────────────

    /** @brief Offscreen framebuffer size in pixels -- the pick buffer's coordinate
     *         space, which is NOT the editor panel's size. */
    virtual void GetFramebufferSize(int32* outWidth, int32* outHeight) const = 0;

    /** @brief Rebuild the per-worker command pools after the job system's thread
     *         count changed. */
    virtual void RecreateWorkerCommandPools() = 0;
};
