#ifndef NOUS_ENGINE_RENDERER_TYPES_H
#define NOUS_ENGINE_RENDERER_TYPES_H

#include "glm/glm.hpp"
#include <vector>

// -----------------------------------------------------------------------------
// Forward declarations
// -----------------------------------------------------------------------------
struct Vertex3D;
class Camera;
class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;

// -----------------------------------------------------------------------------
// Core rendering data
// -----------------------------------------------------------------------------
enum class FrameResult : uint8_t
{
    SUCCESS = 0,       // Frame rendered successfully
    SKIPPED = 1,  // Frame intentionally skipped (e.g., swapchain recreation)
    ERROR = 2     // Fatal failure
};

struct GeometryRenderData
{
    GeometryRenderData() : model(1.0f), geometry(nullptr), material(nullptr) {}

    glm::mat4 model;
    ResourceMesh* geometry;
    ResourceMaterial* material;
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

    // Lifecycle
    [[nodiscard]] virtual bool Initialize() = 0;
    virtual void Shutdown() noexcept = 0;
    virtual void Resized(uint16_t width, uint16_t height) noexcept = 0;

    // Frame lifecycle
    [[nodiscard]] virtual FrameResult BeginFrame(float dt) = 0;
    [[nodiscard]] virtual FrameResult EndFrame(float dt) = 0;

    // Render passes
    [[nodiscard]] virtual bool BeginRenderpass(RenderpassType renderpassID) = 0;
    [[nodiscard]] virtual bool EndRenderpass(RenderpassType renderpassID) = 0;

    // Global states
    [[nodiscard]] virtual bool UpdateGlobalWorldState(
            RenderpassType renderpassID,
            const glm::mat4& projection, const glm::mat4& view,
            const glm::vec3& viewPosition, const glm::vec4& ambientColor,
            int32_t mode) = 0;

    [[nodiscard]] virtual bool UpdateGlobalUIState(
            RenderpassType renderpassID,
            const glm::mat4& projection, const glm::mat4& view,
            int32_t mode) = 0;

    // Drawing
    [[nodiscard]] virtual bool DrawGeometry(
            RenderpassType renderpassID,
            const GeometryRenderData& renderData) = 0;

    // Textures
    [[nodiscard]] virtual bool CreateTexture(const uint8_t* pixels, ResourceTexture* outTexture) = 0;
    virtual void DestroyTexture(ResourceTexture* texture) noexcept = 0;

    // Materials
    [[nodiscard]] virtual bool CreateMaterial(ResourceMaterial* material) = 0;
    virtual void DestroyMaterial(ResourceMaterial* material) noexcept = 0;

    // Geometry
    [[nodiscard]] virtual bool CreateGeometry(
            uint32_t vertexCount, const Vertex3D* vertices,
            uint32_t indexCount, const uint32_t* indices,
            ResourceMesh* outGeometry) = 0;
    virtual void DestroyGeometry(ResourceMesh* geometry) noexcept = 0;
};

#endif // NOUS_ENGINE_RENDERER_TYPES_H