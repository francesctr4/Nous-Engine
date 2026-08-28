#include <ECS/Component/Types/CVideoPlayer/CVideoPlayer.h>
#include <EngineCore/Casts.h>

#include <FileSystem/FileSystem.h>   // GetFilename
#include <ECS/ComponentServices.h>
#include <ECS/Scene/iSceneHost.h>
#include <VideoSystem/iVideoBroker.h>
#include <ResourceManager/Core/IResourceLoader.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CAudioSource/CAudioSource.h>
#include "Component/Types/CVideoPlayer/VideoPlayhead.h"
#include <ResourceManager/Core/ResourceBase.h>
#include <ResourceManager/Types/ResourceVideo/ResourceVideo.h>
#include <ResourceManager/Types/ResourceType.h>
#include <Utils/Serialization/JsonObject.h>

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void CVideoPlayer::OnUpdate(float deltaTime)
{
    const ComponentServices& s = Services();
    if (!s.host || !s.video)
        return;  // headless / test scene — no video broker wired

    // STOPPED — tear the decoder down so the next play session starts cleanly.
    if (s.host->IsStopped())
    {
        if (handle)
        {
            s.video->DestroyVideo(handle);
            handle = nullptr;
        }
        playhead    = 0.0;
        frameDirty  = false;
        latestFrame = VideoFrame{};
        m_started   = false;
        return;
    }

    // PAUSED — hold the playhead; deliver nothing new. Note this holds the decoder
    // too: only STOPPED tears it down, so resuming does not restart the clip.
    if (s.host->IsPaused())
        return;

    // PLAYING.
    if (!m_started)
    {
        if (playOnAwake && clip)
        {
            // Force PREDECODED for a seamless loop if the user opted in. The backend reads
            // GetDecodeMode() inside CreateVideo, so setting it here (just before) wins over
            // the extension policy ImporterVideo applied at import (.mp4 -> STREAMED).
            if (predecode)
                clip->SetDecodeMode(VideoDecodeMode::PREDECODED);
            if (!handle)
                handle = s.video->CreateVideo(clip);
            if (handle)
            {
                s.video->SetLooping(handle, loop);
                s.video->Start(handle);
            }
        }
        m_started = true;   // don't retry creation every frame when there's no clip
    }

    if (!handle)
        return;

    // Slave the playhead to a sibling CAudioSource's audio clock for A/V sync,
    // gated three ways: (1) the user opted in (syncToAudio) and this clip actually
    // has an audio track — GIFs carry no sound, so they never sync; (2) a sibling
    // CAudioSource exists; (3) its voice is actually playing. If any fails, the
    // video runs on its own dt clock and loops per its own flag. Scene-pause is
    // handled by the early return above.
    auto go = GetGameObject();
    CAudioSource* audioSibling = go.IsValid() ? go.TryGetComponent<CAudioSource>() : nullptr;
    const bool   wantsAudioSync   = syncToAudio && clip && clip->GetHasAudioTrack();
    const bool   audioClockActive = wantsAudioSync && audioSibling && audioSibling->IsVoicePlaying();
    const double audioSeconds      = audioClockActive ? audioSibling->GetPlaybackSeconds() : 0.0;

    playhead = ResolveVideoPlayhead(playhead, static_cast<double>(deltaTime),
                                    static_cast<double>(s.video->GetDuration(handle)), loop,
                                    audioClockActive, audioSeconds);

    if (s.video->TryGetFrame(handle, playhead, latestFrame))
        frameDirty = true;
}

void CVideoPlayer::OnDestroy()
{
    const ComponentServices& s = Services();

    // Release the decoder handle. ModuleVideo is constructed before the scene, so it is
    // guaranteed alive during scene teardown (Application module order: VIDEO before SCENE).
    // No GPU work here — the dynamic texture is owned by the renderer (DynamicTextureCache).
    if (s.video && handle)
        s.video->DestroyVideo(handle);
    handle      = nullptr;
    frameDirty  = false;
    latestFrame = VideoFrame{};
    m_started   = false;

    // Drop our reference to the clip resource (mirrors CMesh / CAudioSource::OnDestroy).
    if (clip && s.resources)
        s.resources->UnloadResource(clip->GetUID());
    clip = nullptr;
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

JsonObject CVideoPlayer::Serialize() const
{
    JsonObject root;
    root.Set("type", GetType());

    root.Set("assetPath", clip ? clip->GetAssetsPath() : "");
    if (clip)
    {
        root.Set("libraryPath", clip->GetLibraryPath());
        root.Set("resourceUID", static_cast<double>(clip->GetUID()));
    }

    root.Set("loop",        loop);
    root.Set("playOnAwake", playOnAwake);
    root.Set("syncToAudio", syncToAudio);
    root.Set("predecode",   predecode);
    root.Set("targetSlot",  targetSlot);
    return root;
}

void CVideoPlayer::Deserialize(const JsonObject& obj)
{
    loop        = obj.GetBool  ("loop",        loop);
    playOnAwake = obj.GetBool  ("playOnAwake", playOnAwake);
    syncToAudio = obj.GetBool  ("syncToAudio", syncToAudio);
    predecode   = obj.GetBool  ("predecode",   predecode);
    targetSlot  = obj.GetString("targetSlot", targetSlot.c_str());

    const std::string assetPath   = obj.GetString("assetPath");
    const std::string libraryPath = obj.GetString("libraryPath");
    const uint32_t      resourceUID = static_cast<uint32_t>(obj.GetDouble("resourceUID", 0.0));

    if (assetPath.empty() && libraryPath.empty())
        return;  // no clip authored

    IResourceLoader* rm = Services().resources;
    if (!rm)
        return;

    // GAME path: load straight from Library without reading a .meta file.
    if (!libraryPath.empty() && resourceUID != 0)
    {
        if (ResourceBase* r = rm->CreateResourceFromLibrary(
                resourceUID, ResourceType::VIDEO,
                nous::engine::filesystem::GetFilename(assetPath), assetPath, libraryPath))
            clip = down_cast<ResourceVideo*>(r);
    }

    // EDITOR path / fallback: resolve via the asset path.
    if (!clip && !assetPath.empty())
    {
        if (ResourceBase* r = rm->CreateResource(assetPath))
            clip = down_cast<ResourceVideo*>(r);
    }
}
