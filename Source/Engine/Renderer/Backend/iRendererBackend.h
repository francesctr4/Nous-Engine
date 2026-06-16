#pragma once

#include "Engine/Renderer/RendererTypes.h"

// Forward declarations for dependency injection
class ModuleWindow;
class ModuleResourceManager;
class EventSystem;
namespace nous::engine::multithreading { class NOUS_JobSystem; }

// -----------------------------------------------------------------------------
// Renderer backend interface
// -----------------------------------------------------------------------------
/**
 * @brief Abstract interface implemented by all renderer backends.
 *
 * Responsibilities:
 *  - Manage GPU device, swapchain, and render lifecycle.
 *  - Manage GPU resources (textures, materials, meshes).
 *  - Handle per-frame rendering and render passes.
 */
struct IRendererBackend
{
    virtual ~IRendererBackend() noexcept = default;

    // ─────────────────────────────── Dependency Injection ────────────────────
    virtual void InjectDependencies(
        EventSystem* eventSystem,
        nous::engine::multithreading::NOUS_JobSystem* jobSystem,
        ModuleWindow* window,
        ModuleResourceManager* resourceManager) = 0;

    // ─────────────────────────────── Lifecycle ───────────────────────────────
    [[nodiscard]] virtual bool Initialize() = 0;
    virtual void Shutdown() noexcept = 0;
    virtual void SetRenderMode(RenderMode mode) noexcept = 0;

    /**
     * @brief Waits for the GPU to finish, then frees command buffers and framebuffers.
     *
     * After this call, no Vulkan object is referenced by any command buffer or
     * framebuffer, so the caller can safely destroy application-level resources
     * (textures, meshes, shaders, ImGui resources, etc.).
     *
     * Idempotent — safe to call more than once.  Shutdown() calls this internally
     * as a safety fallback if it was not called explicitly beforehand.
     */
    virtual void ReleaseFrameResources() noexcept = 0;

    virtual void Resized(uint16_t width, uint16_t height) noexcept = 0;

    // ─────────────────────────────── Frame Lifecycle ─────────────────────────
    [[nodiscard]] virtual FrameResult BeginFrame(float dt) = 0;
    [[nodiscard]] virtual FrameResult EndFrame(float dt) = 0;

    // ─────────────────────────────── Rendering ───────────────────────────────
    [[nodiscard]] virtual bool BeginRenderpass(RenderpassType renderpassID) = 0;
    [[nodiscard]] virtual bool EndRenderpass(RenderpassType renderpassID) = 0;

    [[nodiscard]] virtual bool UpdateGlobalWorldState(
            RenderpassType renderpassID,
            const GlobalUBO& globalUBO) = 0;

    [[nodiscard]] virtual bool DrawGeometry(
            RenderpassType renderpassID,
            const GeometryRenderData& renderData) = 0;

    // instanceOffset: index into the SSBO where writing starts.
    // Scene pass uses offset 0; game pass uses offset c_maxInstances to avoid overwriting scene data.
    // The target SSBO ring slot is chosen internally from the current swapchain image index,
    // matching the static per-image descriptor binding (see WriteInstanceSSBODescriptor).
    virtual void UploadInstanceMatrices(const glm::mat4* matrices,
                                        uint32_t count,
                                        uint32_t instanceOffset) = 0;

    [[nodiscard]] virtual bool DrawGeometryBatched(RenderpassType renderpassID,
                                                    const InstancedBatch& batch) = 0;

    // ─────────────────────────────── Resources ───────────────────────────────
    [[nodiscard]] virtual bool CreateTexture(const uint8_t* pixels, ResourceTexture* outTexture) = 0;
    virtual void DestroyTexture(ResourceTexture* texture) noexcept = 0;

    // Re-upload RGBA8 pixels into an already-created texture's existing GPU image
    // (same VkImage/view/sampler => no descriptor rewrite). For per-frame dynamic
    // textures (video). pixels must be width*height*4 bytes for texture's dimensions.
    [[nodiscard]] virtual bool UpdateDynamicTexture(const uint8_t* pixels, ResourceTexture* texture) = 0;

    [[nodiscard]] virtual bool CreateMaterial(ResourceMaterial* material) = 0;
    virtual void DestroyMaterial(ResourceMaterial* material) noexcept = 0;

    [[nodiscard]] virtual bool CreateGeometry(
            uint32_t vertexCount, const Vertex3D* vertices,
            uint32_t indexCount, const uint32_t* indices,
            ResourceMesh* outGeometry) = 0;
    virtual void DestroyGeometry(ResourceMesh* geometry) noexcept = 0;

    [[nodiscard]] virtual bool CreateShader(ResourceShader* shader) = 0;
    virtual void DestroyShader(ResourceShader* shader) noexcept = 0;

    // Recompile from source and rebuild the GPU pipeline for the given shader.
    // Calls vkDeviceWaitIdle internally. Returns false (and leaves the old shader
    // intact) if compilation fails.
    virtual bool ReloadShader(ResourceShader* shader) noexcept = 0;

    // GPU-swap only — caller has already updated shader->stagesData, shader->reflection,
    // and shader->generation. Calls vkDeviceWaitIdle, then destroys and recreates all
    // GPU resources. Used by the async compile path (RendererFrontend::FlushCompletedReloads).
    virtual bool ApplyCompiledShader(ResourceShader* shader) noexcept = 0;

    // Block until the GPU has finished all in-flight work (vkDeviceWaitIdle).
    // Lighter than ReleaseFrameResources — does NOT free command buffers or framebuffers.
    // Use before destroying GPU resources outside the normal shutdown path (e.g. hot reload).
    virtual void WaitForGPUIdle() noexcept = 0;

    // ─────────────────────────────── Picking ────────────────────────────────
    /**
     * @brief Render the scene to a pick buffer and read back the object ID
     *        at the given pixel coordinate.
     * @return The objectUID at (pixelX, pixelY), or 0 if nothing was hit.
     */
    virtual uint32_t PickObjectAt(int32_t pixelX, int32_t pixelY,
                                  const glm::mat4& projection, const glm::mat4& view,
                                  const std::vector<GeometryRenderData>& geometries) = 0;

    // ─────────────────────────────── Outlining ───────────────────────────────
    /**
     * @brief Render a stencil-based outline around the given geometries.
     *        Only affects RenderpassType::SCENE (editor viewport).
     */
    virtual bool DrawOutlinedGeometries(RenderpassType renderpassID,
                                        const glm::mat4& projection,
                                        const glm::mat4& view,
                                        const std::vector<GeometryRenderData>& outlinedGeometries,
                                        const OutlineSettings& settings) = 0;

    // ─────────────────────────────── Editor Grid ─────────────────────────────
    /**
     * @brief Render a reference grid at the world origin on the XZ plane.
     *        Only draws when renderpassID == SCENE (editor viewport).
     */
    virtual bool DrawGrid(RenderpassType renderpassID,
                          const glm::mat4& projection,
                          const glm::mat4& view) = 0;

    // ─────────────────────────────── Background ──────────────────────────────
    /**
     * @brief Render a fullscreen sky-to-horizon gradient as the viewport background.
     *        Must be called first in the renderpass, before any geometry.
     *        Works for both SCENE and GAME renderpasses.
     */
    virtual bool DrawBackground(RenderpassType renderpassID,
                                const glm::mat4& projection,
                                const glm::mat4& view) = 0;

    // ─────────────────────────────── Wireframe Debug Meshes ──────────────────
    /**
     * @brief Draw N instances of a backend-owned static wireframe mesh
     *        (cube / sphere / pyramid / cone) in the Scene View.
     *        Only draws when renderpassID == SCENE (editor viewport).
     *
     * All variants reuse the bounding-box shader (LINE_LIST, mat4+vec4 push
     * constants) and a shared per-mesh vertex buffer. Each WireframeInstance
     * supplies a world-space transform and a line color. Replaces the former
     * per-purpose DrawBoundingBoxes / DrawPointLightDebugs /
     * DrawDirectionalLightDebugs / DrawSpotLightDebugs entry points.
     *
     * The shared set=0 global descriptor is updated by the first wireframe draw
     * of the frame and only rebound by the rest; the backend tracks this
     * internally (per-frame, reset when the SCENE renderpass begins), so callers
     * issue these draws in any order without bookkeeping.
     */
    virtual bool DrawWireframeMeshInstances(RenderpassType renderpassID,
                                            const glm::mat4& projection,
                                            const glm::mat4& view,
                                            WireframeMesh mesh,
                                            const std::vector<WireframeInstance>& instances) = 0;

    // ─────────────────────────────── Camera Frustums ─────────────────────────
    /**
     * @brief Render camera frustum wireframes in the Scene View.
     *        Only draws when renderpassID == SCENE (editor viewport).
     *
     * Each CameraFrustumData holds the 8 world-space corners of a perspective
     * frustum. 12 edges are drawn (near quad + far quad + 4 connecting lines).
     * Kept separate from DrawWireframeMeshInstances because each frustum's
     * geometry is unique (CPU-built per frame), not an affine transform of a
     * shared mesh. Shares the same backend-tracked set=0 update-once-per-frame
     * behavior as DrawWireframeMeshInstances.
     */
    virtual bool DrawCameraFrustums(RenderpassType renderpassID,
                                    const glm::mat4& projection,
                                    const glm::mat4& view,
                                    const std::vector<CameraFrustumData>& frustums) = 0;
};
