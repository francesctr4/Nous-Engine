#ifndef NOUS_ENGINE_RENDERER_TYPES_H
#define NOUS_ENGINE_RENDERER_TYPES_H

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------
struct Vertex3D;
class Camera;
class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;
class ResourceShader;

// -----------------------------------------------------------------------------
// Texture Maps
// -----------------------------------------------------------------------------
enum class TextureMapType : int8_t
{
    UNKNOWN = -1,
    DIFFUSE
};

struct TextureMap
{
    TextureMap() : type(TextureMapType::UNKNOWN), texture(nullptr) {}

    TextureMapType type;
    ResourceTexture* texture;
};

// -----------------------------------------------------------------------------
// Core rendering data
// -----------------------------------------------------------------------------
enum class FrameResult : uint8_t
{
    SUCCESS = 0,  // Frame rendered successfully
    SKIPPED = 1,  // Frame intentionally skipped (e.g., swapchain recreation)
    ERROR = 2     // Fatal failure
};

struct GeometryRenderData
{
    GeometryRenderData() : objectUID(0), model(1.0f), geometry(nullptr), material(nullptr), color(1.0f) {}

    uint32_t objectUID;
    glm::mat4 model;
    ResourceMesh* geometry;
    ResourceMaterial* material;
    glm::vec4 color;
};

static constexpr uint32_t c_maxInstances = 4096;

struct InstancedBatch
{
    ResourceMesh*     geometry      = nullptr;
    ResourceMaterial* material      = nullptr;
    uint32_t          firstInstance = 0;
    uint32_t          instanceCount = 0;
};

enum class RenderpassType : uint8_t
{
    SCENE,
    GAME,
    UI
};

// -----------------------------------------------------------------------------
// Light data (std140-safe: all members use vec4/ivec4, no vec3)
// -----------------------------------------------------------------------------
constexpr uint32_t c_maxPointLights = 16;
constexpr uint32_t c_maxSpotLights  = 8;

struct DirectionalLight
{
    glm::vec4 direction;  // xyz = world-space direction (normalised), w = unused
    glm::vec4 color;      // xyz = RGB, w = intensity
};

struct PointLight
{
    glm::vec4 position;   // xyz = world-space position, w = range
    glm::vec4 color;      // xyz = RGB, w = intensity
};

struct SpotLight
{
    glm::vec4 position;   // xyz = world-space position, w = range
    glm::vec4 direction;  // xyz = world-space direction (normalised), w = unused
    glm::vec4 color;      // xyz = RGB, w = intensity
    glm::vec4 angles;     // x = cos(innerAngle), y = cos(outerAngle), zw = unused
};
// 64 bytes, std140-safe (all vec4)

// GPU-side global uniform buffer object.
// Layout must mirror the GLSL GlobalUBO block exactly (std140).
// IMPORTANT: Never use glm::vec3 here — in std140, vec3 has 16-byte alignment
// while glm::vec3 has 4-byte alignment in C++, causing silent layout mismatch.
// Use glm::ivec4 for the count field so both sides agree on 16-byte size + alignment.
struct GlobalUBO
{
    glm::mat4        projection;                    //  64 bytes, offset   0
    glm::mat4        view;                          //  64 bytes, offset  64
    glm::vec4        viewPosition;                  //  16 bytes, offset 128 (w = unused)
    glm::vec4        ambientColor;                  //  16 bytes, offset 144 (w = intensity)
    DirectionalLight directionalLight;              //  32 bytes, offset 160
    glm::ivec4       lightCountAndPad;              //  16 bytes, offset 192 (x = activePointLightCount)
    PointLight       pointLights[c_maxPointLights]; // 512 bytes, offset 208
    glm::vec4        time;                            //  16 bytes, offset 720 (x=totalTime, y=sin(t), z=cos(t), w=deltaTime)
    glm::ivec4       spotLightCountAndPad;            //  16 bytes, offset 736 (x = activeSpotLightCount)
    SpotLight        spotLights[c_maxSpotLights];     // 512 bytes, offset 752
};                                                   // = 1264 bytes total
static_assert(sizeof(GlobalUBO) == 1264,
    "GlobalUBO size changed — update the GLSL block and this assert together.");

struct RenderPacket
{
    RenderPacket() : editorCamera(nullptr), gameCamera(nullptr), deltaTime(0.0f), totalTime(0.0f) {}

    Camera* editorCamera;
    Camera* gameCamera;
    float deltaTime;
    float totalTime;  // seconds since app start (accumulated dt)

    // Scene-pass list: culled against editor camera frustum (EDITOR) or unculled (GAME).
    // Also used for mouse picking and outlined geometry selection.
    std::vector<GeometryRenderData> geometries;

    // Game-pass list: culled against game camera frustum.
    // Only populated in EDITOR mode; GAME mode reuses `geometries` for its sole pass.
    std::vector<GeometryRenderData> gameGeometries;

    DirectionalLight directionalLight              = {};
    bool             hasDirectionalLight           = false;
    PointLight       pointLights[c_maxPointLights] = {};
    uint32_t         activePointLightCount         = 0;
    SpotLight        spotLights[c_maxSpotLights]   = {};
    uint32_t         activeSpotLightCount          = 0;
};

struct OutlineSettings
{
    OutlineSettings() : color(1.f, 0.5f, 0.f, 1.f), width(0.01f), depthAware(false) {}

    glm::vec4 color;
    float width;
    bool depthAware;
};

// -----------------------------------------------------------------------------
// Camera frustum data (for Scene View debug visualization)
// -----------------------------------------------------------------------------
/**
 * @brief 8-corner world-space frustum for a CCamera component.
 *
 * Corner order:
 *   [0] near TL, [1] near TR, [2] near BR, [3] near BL
 *   [4] far  TL, [5] far  TR, [6] far  BR, [7] far  BL
 */
struct CameraFrustumData
{
    CameraFrustumData() : corners{}, color(0.0f, 1.0f, 0.0f, 1.0f) {}
    CameraFrustumData(const glm::vec3 c[8], const glm::vec4& col)
        : corners{}, color(col) { for (int i = 0; i < 8; ++i) corners[i] = c[i]; }

    glm::vec3 corners[8];
    glm::vec4 color;
};

// -----------------------------------------------------------------------------
// Bounding box data
// -----------------------------------------------------------------------------
/**
 * @brief Transform and color for one bounding box draw call.
 *
 * The transform maps a unit cube (±0.5 on each axis) to the desired
 * bounding box in world space:
 *   - AABB: translate(worldCenter) * scale(worldExtents)  — no rotation
 *   - OBB:  worldMatrix * translate(localCenter) * scale(localExtents)
 */
struct BoundingBoxData
{
    BoundingBoxData() : transform(1.0f), color(1.0f) {}
    BoundingBoxData(const glm::mat4& t, const glm::vec4& c) : transform(t), color(c) {}

    glm::mat4 transform;
    glm::vec4 color;
};

// Per-directional-light debug pyramid data passed to DrawDirectionalLightDebugs().
struct DirectionalLightDebugData
{
    DirectionalLightDebugData() : transform(1.0f), color(1.0f) {}
    DirectionalLightDebugData(const glm::mat4& t, const glm::vec4& c) : transform(t), color(c) {}

    glm::mat4 transform;  // GO position + direction-aligned rotation, no scale
    glm::vec4 color;
};

// Per-spot-light debug cone data passed to DrawSpotLightDebugs().
struct SpotLightDebugData
{
    SpotLightDebugData()
        : markerTransform(1.0f), fullConeTransform(1.0f), color(1.0f), selected(false) {}

    glm::mat4 markerTransform;    // small fixed cone — always drawn
    glm::mat4 fullConeTransform;  // range+angle scaled cone — drawn only when selected
    glm::vec4 color;
    bool      selected;
};

// -----------------------------------------------------------------------------
// Backend shader interface
// -----------------------------------------------------------------------------
/**
 * @brief Opaque interface owned by each ResourceShader.
 *
 * The active backend allocates a concrete implementation (e.g. VulkanShader)
 * and stores it in ResourceShader::internalData. The resource layer never
 * includes any backend headers — it only holds this pointer.
 */
struct IBackendShader
{
    virtual ~IBackendShader() = default;

    virtual void Bind()    = 0;
    virtual void Destroy() = 0;
};

// -----------------------------------------------------------------------------
// Render mode
// -----------------------------------------------------------------------------
enum class RenderMode : uint8_t
{
    EDITOR,  // Default: 3 passes, off-screen viewports, ImGui overlay
    GAME     // Standalone: 1 game pass renders directly to swapchain, no ImGui
};

// -----------------------------------------------------------------------------
// Renderer backend type
// -----------------------------------------------------------------------------
enum class RendererBackendType : int8_t
{
    UNKNOWN = -1,

    VULKAN = 0,
    OPENGL = 1,
    DIRECTX = 2
};

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

#endif // NOUS_ENGINE_RENDERER_TYPES_H