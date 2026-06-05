#pragma once

#include "Engine/Core/Globals.h"

#include <cstdint>
#include <string>
#include <unordered_map>
#include <unordered_set>

class RendererFrontend;
class ResourceTexture;
class ResourceMaterial;
class CVideoPlayer;

// Owns the renderer-side dynamic textures for video surfaces (one per GameObject UID).
// Lives in ModuleRenderer3D so all GPU lifecycle stays renderer-side (the renderer is torn
// down before the scene). Each frame, the drain calls Submit() per dirty CVideoPlayer, then
// Reconcile() to drop surfaces whose player is gone. DestroyAll() runs at shutdown.
class VideoSurfaceCache
{
public:
    // Upload + bind for one active player (called per CVideoPlayer during the drain).
    // `material` is the entity's CMaterial::material (may be null), `goUID` its CEntityInfo id,
    // `defaultMaterial` the ResourceManager shared default (guarded against). Marks goUID live.
    void Submit(RendererFrontend* frontend, uint32 goUID, CVideoPlayer& player,
                ResourceMaterial* material, const ResourceMaterial* defaultMaterial);

    // After visiting all players this frame, destroy surfaces whose goUID was not Submit()ed
    // (player stopped / removed / object deleted). Restores the original slot texture first.
    // Triggers WaitForGPUIdle only when at least one surface is destroyed.
    void Reconcile(RendererFrontend* frontend);

    // Shutdown: destroy every dynamic texture (call after ReleaseFrameResources / GPU idle).
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

    std::unordered_map<uint32, Surface> m_surfaces;      // goUID -> surface
    std::unordered_set<uint32>          m_liveThisFrame; // goUIDs Submit()ed this frame
    std::unordered_set<std::string>     m_warned;        // one-time guard warnings (key = reason+goUID)
};
