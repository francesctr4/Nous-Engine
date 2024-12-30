#pragma once

#include "Globals.h"

#include "Camera.h"
#include "MathUtils.h"
#include "ResourceTypes.inl"

struct GeometryRenderData
{
    Geometry* geometry;
    float4x4 model;
};

struct RenderPacket
{
    Camera camera;
    float deltaTime;

    std::vector<GeometryRenderData> geometries;
};

struct GlobalUniformObject
{
    float4x4 projection;   // 64 bytes
    float4x4 view;         // 64 bytes

    float4x4 m_reserved0;  // 64 bytes, reserved for future use
    float4x4 m_reserved1;  // 64 bytes, reserved for future use
};

struct MaterialUniformObject 
{
    float4 diffuseColor;   // 16 bytes

    float4 v_reserved0;     // 16 bytes, reserved for future use
    float4 v_reserved1;     // 16 bytes, reserved for future use
    float4 v_reserved2;     // 16 bytes, reserved for future use
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

    virtual void UpdateGlobalState(float4x4 projection, float4x4 view, float3 viewPosition, float4 ambientColor, int32 mode) = 0;
    virtual void DrawGeometry(GeometryRenderData renderData) = 0;

    // ---------------------------------------------------------------------------------------------------- //

    virtual void CreateTexture(const uint8* pixels, Texture* outTexture) = 0;
    virtual void DestroyTexture(Texture* texture) = 0;

    // ---------------------------------------------------------------------------------------------------- //

    virtual bool CreateMaterial(Material* material) = 0;
    virtual void DestroyMaterial(Material* material) = 0;

    // ---------------------------------------------------------------------------------------------------- //

    virtual bool CreateGeometry(uint32 vertexCount, const Vertex* vertices, uint32 indexCount, const uint32* indices, Geometry* outGeometry) = 0;
    virtual void DestroyGeometry(Geometry* geometry) = 0;
};