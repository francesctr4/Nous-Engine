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
    virtual void UploadInstanceMatrices(uint32_t frameIndex,
                                        const glm::mat4* matrices,
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

    // ─────────────────────────────── Bounding Boxes ──────────────────────────
    /**
     * @brief Render wireframe bounding boxes (AABB and/or OBB) for debugging.
     *        Only draws when renderpassID == SCENE (editor viewport).
     *
     * Each BoundingBoxData carries a pre-computed transform that maps the
     * unit cube vertex buffer to the desired bounding box in world space.
     */
    virtual bool DrawBoundingBoxes(RenderpassType renderpassID,
                                   const glm::mat4& projection,
                                   const glm::mat4& view,
                                   const std::vector<BoundingBoxData>& boxes) = 0;

    // ─────────────────────────────── Camera Frustums ─────────────────────────
    /**
     * @brief Render camera frustum wireframes in the Scene View.
     *        Only draws when renderpassID == SCENE (editor viewport).
     *
     * Each CameraFrustumData holds the 8 world-space corners of a perspective
     * frustum. 12 edges are drawn (near quad + far quad + 4 connecting lines).
     */
    /**
     * @param globalAlreadySet  Pass true when the bounding-box shader's global
     *        descriptor set was already updated this frame (e.g. DrawBoundingBoxes ran).
     *        When true the function skips vkUpdateDescriptorSets and only rebinds,
     *        avoiding the "descriptor set updated while bound" validation error.
     */
    virtual bool DrawCameraFrustums(RenderpassType renderpassID,
                                    const glm::mat4& projection,
                                    const glm::mat4& view,
                                    const std::vector<CameraFrustumData>& frustums,
                                    bool globalAlreadySet = false) = 0;

    // ─────────────────────────────── Point Light Debugs ──────────────────────
    /**
     * @brief Render wireframe debug spheres for point lights in the Scene View.
     *        Only draws when renderpassID == SCENE (editor viewport).
     *
     * Reuses the bounding-box shader (LINE_LIST, mat4+vec4 push constants) and
     * a shared unit-sphere vertex buffer. Each BoundingBoxData carries a
     * translate+scale transform and the light color.
     *
     * @param globalAlreadySet  Pass true when the bounding-box shader's global
     *        descriptor set was already updated this frame (e.g. DrawBoundingBoxes
     *        or DrawCameraFrustums ran). Avoids "descriptor set updated while
     *        bound" validation errors.
     */
    virtual bool DrawPointLightDebugs(RenderpassType renderpassID,
                                      const glm::mat4& projection,
                                      const glm::mat4& view,
                                      const std::vector<BoundingBoxData>& lightDebugs,
                                      bool globalAlreadySet = false) = 0;

    // ─────────────────────────────── Directional Light Debugs ────────────────
    /**
     * @brief Render wireframe debug pyramids for directional lights in the Scene View.
     *        Only draws when renderpassID == SCENE.
     * @param globalAlreadySet  Pass true when another scenery draw already updated
     *        the bounding-box shader's global descriptor set this frame.
     */
    virtual bool DrawDirectionalLightDebugs(RenderpassType renderpassID,
                                            const glm::mat4& projection,
                                            const glm::mat4& view,
                                            const std::vector<DirectionalLightDebugData>& lightDebugs,
                                            bool globalAlreadySet = false) = 0;

    // ─────────────────────────────── Spot Light Debugs ───────────────────────
    /**
     * @brief Render wireframe debug cones for spot lights in the Scene View.
     *        Only draws when renderpassID == SCENE.
     *        Draws a small marker cone always and a full-scale cone when selected.
     * @param globalAlreadySet  Pass true when another scenery draw already updated
     *        the bounding-box shader's global descriptor set this frame.
     */
    virtual bool DrawSpotLightDebugs(RenderpassType renderpassID,
                                     const glm::mat4& projection,
                                     const glm::mat4& view,
                                     const std::vector<SpotLightDebugData>& lightDebugs,
                                     bool globalAlreadySet = false) = 0;
};
