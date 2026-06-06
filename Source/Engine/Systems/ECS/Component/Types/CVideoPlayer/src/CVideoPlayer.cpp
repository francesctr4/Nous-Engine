#include "Engine/Systems/ECS/Component/Types/CVideoPlayer/include/CVideoPlayer.h"

#include "Engine/Core/Globals.h"                 // down_cast
#include "Engine/Core/FileSystem/FileSystem.h"   // GetFilename
#include "Engine/Modules/ModuleVideo/include/ModuleVideo.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ECS/Component/Types/CAudioSource/include/CAudioSource.h"
#include "Engine/Systems/ECS/Component/Types/CVideoPlayer/include/VideoPlayhead.h"
#include "Engine/Systems/ResourceManager/Core/ResourceBase/include/ResourceBase.h"
#include "Engine/Systems/ResourceManager/Types/ResourceVideo/include/ResourceVideo.h"
#include "Engine/Systems/ResourceManager/Types/ResourceType.h"
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

// ---------------------------------------------------------------------------
// Broker access
// ---------------------------------------------------------------------------

CVideoPlayer::SceneBroker CVideoPlayer::ResolveBroker() const
{
    SceneBroker broker;
    auto go = GetGameObject();
    if (!go.IsValid())
        return broker;

    broker.scene       = go.GetScene();
    broker.moduleScene = broker.scene ? broker.scene->GetModuleScene() : nullptr;
    broker.video       = broker.moduleScene ? broker.moduleScene->GetVideo() : nullptr;
    return broker;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void CVideoPlayer::OnUpdate(float deltaTime)
{
    const SceneBroker broker = ResolveBroker();
    ModuleScene* moduleScene = broker.moduleScene;
    ModuleVideo* video       = broker.video;
    if (!video)
        return;  // headless / test scene, or invalid GameObject — no video broker wired
    // video != null guarantees moduleScene != null (derived from it).

    // STOPPED — tear the decoder down so the next play session starts cleanly.
    if (moduleScene->IsStopped())
    {
        if (handle)
        {
            video->DestroyVideo(handle);
            handle = nullptr;
        }
        playhead    = 0.0;
        frameDirty  = false;
        latestFrame = VideoFrame{};
        m_started   = false;
        return;
    }

    // PAUSED — hold the playhead; deliver nothing new.
    if (moduleScene->IsPaused())
        return;

    // PLAYING.
    if (!m_started)
    {
        if (playOnAwake && clip)
        {
            if (!handle)
                handle = video->CreateVideo(clip);
            if (handle)
            {
                video->SetLooping(handle, loop);
                video->Start(handle);
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
                                    static_cast<double>(video->GetDuration(handle)), loop,
                                    audioClockActive, audioSeconds);

    if (video->TryGetFrame(handle, playhead, latestFrame))
        frameDirty = true;
}

void CVideoPlayer::OnDestroy()
{
    const SceneBroker broker = ResolveBroker();

    // Release the decoder handle. ModuleVideo is constructed before the scene, so it is
    // guaranteed alive during scene teardown (Application module order: VIDEO before SCENE).
    // No GPU work here — the dynamic texture is owned by the renderer (DynamicTextureCache).
    if (broker.video && handle)
        broker.video->DestroyVideo(handle);
    handle      = nullptr;
    frameDirty  = false;
    latestFrame = VideoFrame{};
    m_started   = false;

    // Drop our reference to the clip resource (mirrors CMesh / CAudioSource::OnDestroy).
    if (clip && broker.scene && broker.scene->GetResourceManager())
        broker.scene->GetResourceManager()->UnloadResource(clip->GetUID());
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
    root.Set("targetSlot",  targetSlot);
    return root;
}

void CVideoPlayer::Deserialize(const JsonObject& obj)
{
    loop        = obj.GetBool  ("loop",        loop);
    playOnAwake = obj.GetBool  ("playOnAwake", playOnAwake);
    syncToAudio = obj.GetBool  ("syncToAudio", syncToAudio);
    targetSlot  = obj.GetString("targetSlot", targetSlot.c_str());

    const std::string assetPath   = obj.GetString("assetPath");
    const std::string libraryPath = obj.GetString("libraryPath");
    const uint32      resourceUID = static_cast<uint32>(obj.GetDouble("resourceUID", 0.0));

    if (assetPath.empty() && libraryPath.empty())
        return;  // no clip authored

    auto go = GetGameObject();
    Scene* scene = go.IsValid() ? go.GetScene() : nullptr;
    ModuleResourceManager* rm = scene ? scene->GetResourceManager() : nullptr;
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
