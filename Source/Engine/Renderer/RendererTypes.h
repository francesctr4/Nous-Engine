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
enum class TextureMapType
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

enum class RenderpassType
{
    SCENE,
    GAME,
    UI
};

struct RenderPacket
{
    RenderPacket() : editorCamera(nullptr), gameCamera(nullptr), deltaTime(0.0f) {}

    Camera* editorCamera;
    Camera* gameCamera;
    float deltaTime;

    std::vector<GeometryRenderData> geometries;
};

struct OutlineSettings
{
    OutlineSettings() : color(0.f, 0.5f, 1.f, 1.f), width(3.0f), depthAware(false) {}

    glm::vec4 color;
    float width;
    bool depthAware;
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
// Renderer backend type
// -----------------------------------------------------------------------------
enum class RendererBackendType
{
    UNKNOWN = -1,

    VULKAN = 0,
    OPENGL = 1,
    DIRECTX = 2
};

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

    // ─────────────────────────────── Lifecycle ───────────────────────────────
    [[nodiscard]] virtual bool Initialize() = 0;
    virtual void Shutdown() noexcept = 0;

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
            const glm::mat4& projection, const glm::mat4& view,
            const glm::vec3& viewPosition, const glm::vec4& ambientColor,
            int32_t mode) = 0;

    [[nodiscard]] virtual bool DrawGeometry(
            RenderpassType renderpassID,
            const GeometryRenderData& renderData) = 0;

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
};

#endif // NOUS_ENGINE_RENDERER_TYPES_H