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
    GeometryRenderData() : objectUID(0), model(1.0f), geometry(nullptr), material(nullptr), color(1.0f),
                           palette(nullptr) {}

    uint32_t objectUID;
    glm::mat4 model;
    ResourceMesh* geometry;
    ResourceMaterial* material;
    glm::vec4 color;

    // Borrowed bone palette for this frame, owned by the CAnimator on the owning
    // GameObject's parent. Null means "not skinned".
    //
    // ModuleRenderer3D evaluates the full skinned test once — mesh->hasSkinning,
    // an animator on the parent, and a non-empty palette — and sets this only when
    // it passes. So GroupGeometries branches on the pointer ALONE and never names
    // an ECS type, which is what keeps it a pure function and keeps ECS knowledge
    // at the module layer. Valid for the frame only.
    const std::vector<glm::mat4>* palette;
};

static constexpr uint32_t c_maxInstances = 4096;

// Total bone matrices uploadable per pass. 4096 is roughly 62 Mixamo rigs (66 bones)
// visible simultaneously; past it a character falls back to bind pose with a warning
// rather than reading past the buffer.
static constexpr uint32_t c_maxSkinnedBones = 4096;

// Per-instance palette base meaning "this instance is not skinned". The shader
// checks it BEFORE touching the palette buffer, which is what makes a rigged mesh
// with no bound animator safe — its weights are non-zero, so a weights-only test
// would index into a palette that was never uploaded.
static constexpr uint32_t c_noSkinPalette = 0xFFFFFFFFu;

// The palette SSBO is divided into four fixed regions of c_maxSkinnedBones matrices.
// Each pass packs independently: the scene pass orders by (material, mesh) while the
// per-object pick and outline passes iterate natural order, so their bases cannot be
// shared even though outlined objects are a subset of scene objects.
static constexpr uint32_t c_paletteRegionScene   = 0;
static constexpr uint32_t c_paletteRegionGame    = 1 * c_maxSkinnedBones;
static constexpr uint32_t c_paletteRegionOutline = 2 * c_maxSkinnedBones;
static constexpr uint32_t c_paletteRegionPick    = 3 * c_maxSkinnedBones;
static constexpr uint32_t c_paletteRegionCount   = 4;

// 4096 segments — sized to the normals overlay's display budget, which is what
// bounds this buffer in practice (see k_MaxNormalSegments in ModuleRenderer3D; a
// static_assert there keeps the two in step). Deliberately NOT sized to what the
// buffer could hold: at 20k segments the overlay is an unreadable mass of lines
// that costs several MB of upload per frame to draw.
//
// Vertex3D rather than a leaner debug vertex so the shared bounding-box shader's
// vertex input description applies unchanged.
static constexpr uint32_t c_maxDebugLineVertices = 8192;

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

// Number of RenderpassType values. Global set=0 resources are allocated per pass
// per image, and the pass dimension is indexed by the enum value directly — no
// mapping table to fall out of sync with the enum.
static constexpr uint32_t c_renderpassCount = 3;

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
// Wireframe debug overlays (Scene View)
// -----------------------------------------------------------------------------
/**
 * @brief Identifies one of the backend-owned static wireframe debug meshes.
 *
 * Each value maps to a shared LINE_LIST vertex buffer created once at backend
 * init. DrawWireframeMeshInstances() draws the selected mesh once per instance,
 * applying the instance's transform + color via push constants.
 *
 * Conventional usage (the geometry, not the purpose, is what's named):
 *   Cube    — axis-aligned / oriented bounding boxes
 *   Sphere  — point-light position markers and range spheres
 *   Pyramid — directional-light direction indicators
 *   Cone    — spot-light marker and full-angle cones
 *   Bone    — skeleton bones (a Maya-style tapered shard, oriented per instance)
 *   Joint   — skeleton joint markers
 *
 * NOTE: a value identifies an INSTANCE CHANNEL, and two channels may share the
 * same geometry. Joint and Sphere both draw the unit sphere; they are separate
 * values because RendererFrontend keeps exactly one instance vector per value and
 * SetWireframeInstances REPLACES it, so joints and point-light markers sharing a
 * value would mean two unrelated builders fighting over one vector.
 */
enum class WireframeMesh : uint8_t
{
    Cube,
    Sphere,
    Pyramid,
    Cone,
    Bone,
    Joint,

    COUNT
};

/**
 * @brief One instanced wireframe draw: a transform mapping the shared mesh into
 *        world space, plus a line color.
 *
 * The transform maps the unit mesh to the desired placement in world space, e.g.
 * for a Cube bounding box:
 *   - AABB: translate(worldCenter) * scale(worldExtents)  — no rotation
 *   - OBB:  worldMatrix * translate(localCenter) * scale(localExtents)
 */
struct WireframeInstance
{
    WireframeInstance() : transform(1.0f), color(1.0f) {}
    WireframeInstance(const glm::mat4& t, const glm::vec4& c) : transform(t), color(c) {}

    glm::mat4 transform;
    glm::vec4 color;
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