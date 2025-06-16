#pragma once

#include "Globals.h"

#include "Camera.h"
#include "MathUtils.h"
#include "Vertex.inl"

class ResourceMesh;
class ResourceMaterial;
class ResourceTexture;

struct GeometryRenderData
{
    ResourceMesh* geometry;
    float4x4 model;
};

enum class BuiltInRenderpass
{
    SCENE,
    GAME,
    UI
};

struct RenderPacket
{
    Camera editorCamera;
    Camera gameCamera;
    float deltaTime;

    std::vector<GeometryRenderData> geometries;
};

enum class RendererBackendType
{
    UNKNOWN = -1,

    VULKAN,
    OPENGL,
    DIRECTX
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

    virtual void UpdateGlobalWorldState(BuiltInRenderpass renderpassID, float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode) = 0;
    virtual void UpdateGlobalUIState(BuiltInRenderpass renderpassID, float4x4 projection, float4x4 view, int32 mode) = 0;

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