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

#endif // NOUS_ENGINE_RENDERER_TYPES_H