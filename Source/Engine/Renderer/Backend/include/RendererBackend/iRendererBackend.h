#pragma once

#include <Renderer/RendererTypes.h>

// Forward declarations for dependency injection
class IRenderWindow;
class IRenderResourceProvider;
class IEditorRenderBridge;
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
        IRenderWindow* window,
        IRenderResourceProvider* resourceProvider) = 0;

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

    // ─────────────────────────────── Editor Bridge ───────────────────────────
    /**
     * @brief The narrow, editor-facing view of this backend's GPU state.
     *
     * Only the editor's ImGui layer uses it, to bind ImGui to the live device and
     * to sample the offscreen viewport images. Returning it from the backend --
     * rather than exposing a static context accessor -- is what keeps that access
     * injected instead of ambient. May return nullptr for a backend with no
     * editor support. See Renderer/iEditorRenderBridge.h.
     */
    [[nodiscard]] virtual IEditorRenderBridge* GetEditorBridge() noexcept = 0;

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

    // Uploads one pass's per-instance data: model matrices, the parallel per-instance
    // bone-palette bases, and the concatenated bone palettes themselves.
    //
    // instanceOffset: index into the instance/base SSBOs where writing starts —
    //   0 for the scene pass, c_maxInstances for the game pass.
    // paletteOffset:  index into the bone-palette SSBO —
    //   0 for the scene pass, c_maxSkinnedBones for the game pass.
    //
    // The target SSBO ring slot is chosen internally from the current swapchain image
    // index, matching the static per-image descriptor binding
    // (see WriteGlobalStorageDescriptors).
    virtual void UploadInstanceData(const glm::mat4* matrices,
                                    const uint32_t*  paletteBases,
                                    uint32_t         count,
                                    uint32_t         instanceOffset,
                                    const glm::mat4* palettes,
                                    uint32_t         boneCount,
                                    uint32_t         paletteOffset) = 0;

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

    /**
     * @brief Draws arbitrary world-space line segments in a SINGLE draw call.
     *        Only draws when renderpassID == SCENE (editor viewport).
     *
     * `vertices` is a LINE_LIST whose positions are already in world space, so the
     * model push constant is identity. Silently skips a batch that exceeds the
     * buffer capacity.
     *
     * Separate from DrawWireframeMeshInstances because that family issues one push
     * constant and one draw call PER INSTANCE -- fine for a few dozen light gizmos,
     * ~60k draw calls for one character's per-vertex normals. This follows
     * DrawCameraFrustums instead: build the segments on the CPU, upload once, draw
     * once. Shares the same backend-tracked set=0 update-once-per-frame behavior.
     */
    virtual bool DrawDebugLines(RenderpassType renderpassID,
                                const glm::mat4& projection,
                                const glm::mat4& view,
                                const std::vector<Vertex3D>& vertices,
                                const glm::vec4& color) = 0;
};
