#ifndef NOUS_ENGINE_RENDERER_TYPES_H
#define NOUS_ENGINE_RENDERER_TYPES_H

#include "Core/Globals.h"

#include <vector>
#include <glm/glm.hpp>

struct Vertex3D;
class Camera;
class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;

struct GeometryRenderData
{
    glm::mat4x4 model;
    ResourceMesh* geometry;
    ResourceMaterial* material;
};

enum class BuiltInRenderpass
{
    SCENE,
    GAME,
    UI
};

struct RenderPacket
{
    Camera* editorCamera;
    Camera* gameCamera;
    float deltaTime;

    std::vector<GeometryRenderData> geometries;
};

enum class RendererBackendType
{
    UNKNOWN = -1,

    VULKAN = 0,
    OPENGL = 1,
    DIRECTX = 2
};

/**
 * @brief Interface to implement by all the Renderer Backends
 */
struct IRendererBackend
{
    virtual ~IRendererBackend() = default;

    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;

    virtual void Resized(uint16 width, uint16 height) = 0;

    virtual bool BeginFrame(float dt) = 0;
    virtual bool EndFrame(float dt) = 0;

    virtual bool BeginRenderpass(BuiltInRenderpass renderpassID) = 0;
    virtual bool EndRenderpass(BuiltInRenderpass renderpassID) = 0;

    virtual void UpdateGlobalWorldState(BuiltInRenderpass renderpassID, glm::mat4x4 projection, glm::mat4x4 view, glm::vec3 viewPosition, glm::vec4 ambientColor, int32 mode) = 0;
    virtual void UpdateGlobalUIState(BuiltInRenderpass renderpassID, glm::mat4x4 projection, glm::mat4x4 view, int32 mode) = 0;

    virtual void DrawGeometry(BuiltInRenderpass renderpassID, GeometryRenderData renderData) = 0;

    // ---------------------------------------------------------------------------------------------------- //

    virtual void CreateTexture(const uint8* pixels, ResourceTexture* outTexture) = 0;
    virtual void DestroyTexture(ResourceTexture* texture) = 0;

    // ---------------------------------------------------------------------------------------------------- //

    virtual bool CreateMaterial(ResourceMaterial* material) = 0;
    virtual void DestroyMaterial(ResourceMaterial* material) = 0;

    // ---------------------------------------------------------------------------------------------------- //

    virtual bool CreateGeometry(uint32 vertexCount, const Vertex3D* vertices, uint32 indexCount, const uint32* indices, ResourceMesh* outGeometry) = 0;
    virtual void DestroyGeometry(ResourceMesh* geometry) = 0;
};

#endif // NOUS_ENGINE_RENDERER_TYPES_H