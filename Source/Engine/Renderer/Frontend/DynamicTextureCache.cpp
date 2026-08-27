#include "Engine/Renderer/Frontend/DynamicTextureCache.h"

#include <Logger/Logger.h>
#include <MemoryManager/MemoryManager.h>
#include "Engine/Renderer/Frontend/RendererFrontend.h"
#include "Engine/Renderer/RendererTypes.h"   // TextureMap
#include "Engine/Systems/ResourceManager/Types/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Types/ResourceTexture/include/ResourceTexture.h"

#include <string>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_RENDERER_FRONTEND;

bool DynamicTextureCache::Submit(RendererFrontend* frontend, uint32 objectUID,
                                 const uint8_t* pixels, uint32 width, uint32 height,
                                 const std::string& targetSlot,
                                 ResourceMaterial* material, const ResourceMaterial* defaultMaterial)
{
    m_liveThisFrame.insert(objectUID);

    if (!pixels)
        return false;   // surface still live, but no new frame to upload this call

    // Guards — warn once per objectUID+reason, then skip (no crash).
    auto warnOnce = [&](const char* reason)
    {
        const std::string key = std::string(reason) + ":" + std::to_string(objectUID);
        if (m_warned.insert(key).second)
            NOUS_WARN_C(CURRENT_CHANNEL, "[DynamicSurface] object %u: %s", objectUID, reason);
    };

    if (!material)                   { warnOnce("no CMaterial to bind the dynamic texture into"); return false; }
    if (material == defaultMaterial) { warnOnce("bound to the shared default material - assign a unique material"); return false; }

    // NOTE: we do NOT require textureMaps to already contain targetSlot. The renderer enumerates
    // samplers from SHADER REFLECTION and looks each up in textureMaps, and the importer only
    // creates a textureMaps entry when a texture is *assigned*. An unbound slot (e.g. diffuseSampler
    // showing "none") simply has no key yet — so we create it below via operator[]. If targetSlot is
    // not a real reflected sampler the renderer never looks it up, so the (unused) entry is harmless.

    Surface& s = m_surfaces[objectUID];

    // First frame for this surface (or a resolution change): (re)create the dynamic texture.
    if (!s.dynTex || s.w != width || s.h != height)
    {
        if (s.dynTex) Destroy(frontend, s);   // resolution changed — rebuild

        s.w = width;
        s.h = height;

        s.dynTex = NOUS_NEW<ResourceTexture>(MemoryTag::RENDERER);
        s.dynTex->width        = s.w;
        s.dynTex->height       = s.h;
        s.dynTex->channelCount = 4;
        s.dynTex->generation   = 0;
        s.dynTex->internalData = nullptr;
        s.dynTex->SetName("DynamicSurface");

        if (!frontend->CreateTexture(pixels, s.dynTex))
        {
            NOUS_DELETE(s.dynTex, MemoryTag::RENDERER);
            m_surfaces.erase(objectUID);
            return false;
        }

        // Capture the original slot texture and bind ours.
        s.boundMaterial   = material;
        s.boundSlot       = targetSlot;
        s.originalSlotTex = material->textureMaps[targetSlot].texture;
        material->textureMaps[targetSlot].texture = s.dynTex;
    }
    else
    {
        frontend->UpdateDynamicTexture(pixels, s.dynTex);
    }

    return true;
}

void DynamicTextureCache::Reconcile(RendererFrontend* frontend)
{
    bool any = false;
    for (auto it = m_surfaces.begin(); it != m_surfaces.end(); )
    {
        if (m_liveThisFrame.find(it->first) == m_liveThisFrame.end())
        {
            if (!any) { frontend->WaitForGPUIdle(); any = true; }  // one idle for the whole batch
            Destroy(frontend, it->second);
            it = m_surfaces.erase(it);
        }
        else ++it;
    }
    m_liveThisFrame.clear();
}

void DynamicTextureCache::DestroyAll(RendererFrontend* frontend)
{
    for (auto& [uid, s] : m_surfaces)
        Destroy(frontend, s);
    m_surfaces.clear();
    m_liveThisFrame.clear();
}

void DynamicTextureCache::Destroy(RendererFrontend* frontend, Surface& s)
{
    // Restore the original slot texture if our dynamic one is still bound there.
    if (s.boundMaterial)
    {
        auto it = s.boundMaterial->textureMaps.find(s.boundSlot);
        if (it != s.boundMaterial->textureMaps.end() && it->second.texture == s.dynTex)
            it->second.texture = s.originalSlotTex;
    }
    if (s.dynTex)
    {
        frontend->DestroyTexture(s.dynTex);             // frees the VulkanImage
        NOUS_DELETE(s.dynTex, MemoryTag::RENDERER);     // frees the ResourceTexture object
    }
    s = Surface{};
}
