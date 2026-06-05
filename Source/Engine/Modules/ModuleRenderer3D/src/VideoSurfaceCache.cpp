#include "Engine/Modules/ModuleRenderer3D/include/VideoSurfaceCache.h"

#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Renderer/Frontend/RendererFrontend.h"
#include "Engine/Renderer/RendererTypes.h"   // TextureMap
#include "Engine/Systems/ECS/Component/Types/CVideoPlayer/include/CVideoPlayer.h"
#include "Engine/Systems/ResourceManager/Types/ResourceMaterial/include/ResourceMaterial.h"
#include "Engine/Systems/ResourceManager/Types/ResourceTexture/include/ResourceTexture.h"

#include <string>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MODULE_RENDERER3D;

void VideoSurfaceCache::Submit(RendererFrontend* frontend, uint32 goUID, CVideoPlayer& player,
                               ResourceMaterial* material, const ResourceMaterial* defaultMaterial)
{
    m_liveThisFrame.insert(goUID);

    if (!player.frameDirty || !player.latestFrame.pixels)
        return;

    // Guards — warn once per goUID+reason, then skip (no crash).
    auto warnOnce = [&](const char* reason)
    {
        const std::string key = std::string(reason) + ":" + std::to_string(goUID);
        if (m_warned.insert(key).second)
            NOUS_WARN_C(CURRENT_CHANNEL, "[Video] GameObject %u: %s", goUID, reason);
    };

    if (!material)                   { warnOnce("no CMaterial on the video object - nothing to bind"); return; }
    if (material == defaultMaterial) { warnOnce("bound to the shared default material - assign a unique material"); return; }
    if (material->textureMaps.find(player.targetSlot) == material->textureMaps.end())
                                     { warnOnce("material has no texture slot matching targetSlot"); return; }

    Surface& s = m_surfaces[goUID];

    // First frame for this surface (or a resolution change): (re)create the dynamic texture.
    if (!s.dynTex || s.w != player.latestFrame.width || s.h != player.latestFrame.height)
    {
        if (s.dynTex) Destroy(frontend, s);   // resolution changed — rebuild

        s.w = player.latestFrame.width;
        s.h = player.latestFrame.height;

        s.dynTex = NOUS_NEW<ResourceTexture>(MemoryTag::VIDEO_SYSTEM);
        s.dynTex->width        = s.w;
        s.dynTex->height       = s.h;
        s.dynTex->channelCount = 4;
        s.dynTex->generation   = 0;
        s.dynTex->internalData = nullptr;
        s.dynTex->SetName("VideoSurface");

        if (!frontend->CreateTexture(player.latestFrame.pixels, s.dynTex))
        {
            NOUS_DELETE(s.dynTex, MemoryTag::VIDEO_SYSTEM);
            m_surfaces.erase(goUID);
            return;
        }

        // Capture the original slot texture and bind ours.
        s.boundMaterial   = material;
        s.boundSlot       = player.targetSlot;
        s.originalSlotTex = material->textureMaps[player.targetSlot].texture;
        material->textureMaps[player.targetSlot].texture = s.dynTex;
    }
    else
    {
        frontend->UpdateDynamicTexture(player.latestFrame.pixels, s.dynTex);
    }

    player.frameDirty = false;
}

void VideoSurfaceCache::Reconcile(RendererFrontend* frontend)
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

void VideoSurfaceCache::DestroyAll(RendererFrontend* frontend)
{
    for (auto& [uid, s] : m_surfaces)
        Destroy(frontend, s);
    m_surfaces.clear();
    m_liveThisFrame.clear();
}

void VideoSurfaceCache::Destroy(RendererFrontend* frontend, Surface& s)
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
        NOUS_DELETE(s.dynTex, MemoryTag::VIDEO_SYSTEM); // frees the ResourceTexture object
    }
    s = Surface{};
}
