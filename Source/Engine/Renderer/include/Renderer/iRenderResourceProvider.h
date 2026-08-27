#pragma once

#include <functional>

// Forward declarations
class ResourceMaterial;
class ResourceShader;
class ResourceTexture;

// -----------------------------------------------------------------------------
// The resource system, as seen from inside Renderer/.
// -----------------------------------------------------------------------------
/**
 * @brief Fallback resources and typed resource iteration, as needed by the renderer.
 *
 * Implemented by ModuleResourceManager so that Renderer/ can substitute default
 * textures/materials and drive shader hot-reload without depending on Modules/.
 * Companion to IGPUResourceFactory, which runs the other way (ModuleResourceManager
 * and the importers depend on it, RendererFrontend implements it).
 *
 * Deliberately NOT the same interface as IResourceLoader: that one is sized to
 * what components and the scene preloader need (create/request/unload), and the
 * two client sets overlap in exactly one method (GetDefaultMaterial). Merging
 * them would hand each consumer the other's surface.
 *
 * The ForEach* methods replace the renderer's use of ModuleResourceManager's
 * GetResourcesMap() (which remains, for the editor and Application). All three
 * renderer call sites did the same thing with it -- iterate, filter on
 * ResourceType, down_cast -- so folding that into the interface keeps the UID
 * map, the type tag, and the cast out of the renderer entirely. It also folds in
 * a null check those call sites were missing: a snapshot of the resource table
 * contains nullptr placeholders for slots that are claimed but still loading.
 */
class IRenderResourceProvider
{
public:
    virtual ~IRenderResourceProvider() = default;

    // ─────────────────────────────── Fallback resources ──────────────────────
    // All borrowed -- do NOT release these. Any may be null before the
    // ResourceManager has finished starting up.

    /** @brief Substituted for a mesh whose material failed to load. */
    [[nodiscard]] virtual ResourceMaterial* GetDefaultMaterial() const = 0;

    /** @brief Magenta checkerboard, substituted for a missing albedo map. */
    [[nodiscard]] virtual ResourceTexture* GetDefaultTexture() const = 0;

    /** @brief Flat tangent-space normal (0.5, 0.5, 1.0), for a missing normal map. */
    [[nodiscard]] virtual ResourceTexture* GetFlatNormalTexture() const = 0;

    /** @brief Opaque black, the neutral value for additive maps (emissive). */
    [[nodiscard]] virtual ResourceTexture* GetBlackTexture() const = 0;

    /** @brief Opaque white, the neutral value for multiplicative maps (AO, roughness). */
    [[nodiscard]] virtual ResourceTexture* GetWhiteTexture() const = 0;

    // ─────────────────────────────── Typed iteration ─────────────────────────
    // Both walk a moment-in-time snapshot and never pass null, so the callback
    // may safely do heavy work (dispatch a compile job, re-acquire a descriptor
    // slot) without holding the resource registry's lock. The flip side of
    // snapshot semantics: a resource unloaded by another thread mid-iteration
    // leaves a dangling pointer in the callback, exactly as the GetResourcesMap
    // loops these replaced already did.

    /** @brief Visits every loaded shader. Used to dispatch hot-reload compiles. */
    virtual void ForEachShader(const std::function<void(ResourceShader*)>& fn) const = 0;

    /** @brief Visits every loaded material. Used to re-acquire descriptor-set
     *         instance slots after a shader's pools are recreated. */
    virtual void ForEachMaterial(const std::function<void(ResourceMaterial*)>& fn) const = 0;
};
