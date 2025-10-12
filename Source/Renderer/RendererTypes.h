#ifndef NOUS_ENGINE_RENDERER_TYPES_H
#define NOUS_ENGINE_RENDERER_TYPES_H

#include "Core/Globals.h"
#include <vector>
#include <glm/glm.hpp>

// Forward declarations
struct Vertex3D;
class Camera;
class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;

// -----------------------------------------------------------------------------
// Core rendering data
// -----------------------------------------------------------------------------
struct GeometryRenderData
{
    glm::mat4 model{ 1.0f };
    ResourceMesh* geometry = nullptr;
    ResourceMaterial* material = nullptr;
};

enum class BuiltInRenderpass
{
    SCENE,
    GAME,
    UI
};

struct RenderPacket
{
    Camera* editorCamera = nullptr;
    Camera* gameCamera = nullptr;
    float deltaTime = 0.0f;

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
    DIRECTX = 2,

    MAX
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
    virtual ~IRendererBackend() = default;

    // Lifecycle
    [[nodiscard]] virtual bool Initialize() = 0;
    virtual void Shutdown() noexcept = 0;
    virtual void Resized(uint16 width, uint16 height) noexcept = 0;

    // Frame lifecycle
    [[nodiscard]] virtual bool BeginFrame(float dt) = 0;
    [[nodiscard]] virtual bool EndFrame(float dt) = 0;

    // Render passes
    [[nodiscard]] virtual bool BeginRenderpass(BuiltInRenderpass renderpassID) = 0;
    [[nodiscard]] virtual bool EndRenderpass(BuiltInRenderpass renderpassID) = 0;

    // Global states
    [[nodiscard]] virtual bool UpdateGlobalWorldState(
            BuiltInRenderpass renderpassID,
            const glm::mat4& projection, const glm::mat4& view,
            const glm::vec3& viewPosition, const glm::vec4& ambientColor,
            int32 mode) = 0;

    [[nodiscard]] virtual bool UpdateGlobalUIState(
            BuiltInRenderpass renderpassID,
            const glm::mat4& projection, const glm::mat4& view,
            int32 mode) = 0;

    // Drawing
    [[nodiscard]] virtual bool DrawGeometry(
            BuiltInRenderpass renderpassID,
            const GeometryRenderData& renderData) = 0;

    // Textures
    [[nodiscard]] virtual bool CreateTexture(const uint8* pixels, ResourceTexture* outTexture) = 0;
    virtual bool DestroyTexture(ResourceTexture* texture) noexcept = 0;

    // Materials
    [[nodiscard]] virtual bool CreateMaterial(ResourceMaterial* material) = 0;
    virtual bool DestroyMaterial(ResourceMaterial* material) noexcept = 0;

    // Geometry
    [[nodiscard]] virtual bool CreateGeometry(
            uint32 vertexCount, const Vertex3D* vertices,
            uint32 indexCount, const uint32* indices,
            ResourceMesh* outGeometry) = 0;

    virtual bool DestroyGeometry(ResourceMesh* geometry) noexcept = 0;
};

#endif // NOUS_ENGINE_RENDERER_TYPES_H