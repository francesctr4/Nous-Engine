#pragma once

#include <EngineCore/Globals.h>

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

class RendererFrontend;
class ResourceTexture;
class ResourceMaterial;

// Owns renderer-side dynamic textures bound into material sampler slots (one per object UID).
// A "dynamic surface" is any per-frame-updated RGBA texture fed by CPU pixels (e.g. video
// playback). It lives in the renderer because the dynamic VulkanImage must be freed while the
// Vulkan device is still alive — the renderer is torn down before the scene, so GPU ownership
// has to stay here, not on the component that produces the pixels.
//
// It knows nothing about ECS / components: callers pass raw pixels + the target material slot.
// Usage per frame: Submit() for each live surface, then Reconcile() to drop surfaces whose UID
// was not submitted. DestroyAll() at shutdown (device still up, owning materials still alive).
class DynamicTextureCache
{
public:
    // Mark `objectUID` live this frame and, when `pixels` is non-null, upload them into a
    // renderer-owned dynamic texture bound into material[targetSlot]. Returns true when the
    // pixels were consumed (created or updated), false when skipped (no pixels / guarded).
    bool Submit(RendererFrontend* frontend, uint32 objectUID,
                const uint8_t* pixels, uint32 width, uint32 height,
                const std::string& targetSlot,
                ResourceMaterial* material, const ResourceMaterial* defaultMaterial);

    // Destroy surfaces whose UID was not Submit()ed this frame (restores their original slot
    // texture first). Triggers WaitForGPUIdle only when at least one surface is destroyed.
    void Reconcile(RendererFrontend* frontend);

    // Shutdown: destroy every dynamic texture. Call after ReleaseFrameResources (GPU idle) and
    // BEFORE the owning materials are torn down (Destroy restores the original slot pointer).
    void DestroyAll(RendererFrontend* frontend);

private:
    struct Surface
    {
        ResourceTexture*  dynTex          = nullptr;  // renderer-owned; NOT a ResourceManager resource
        ResourceMaterial* boundMaterial   = nullptr;
        std::string       boundSlot;
        ResourceTexture*  originalSlotTex = nullptr;  // captured to restore on teardown
        uint32            w = 0, h = 0;
    };

    void Destroy(RendererFrontend* frontend, Surface& s);

    std::unordered_map<uint32, Surface> m_surfaces;      // objectUID -> surface
    std::unordered_set<uint32>          m_liveThisFrame; // UIDs Submit()ed this frame
    std::unordered_set<std::string>     m_warned;        // one-time guard warnings (key = reason+UID)
};
