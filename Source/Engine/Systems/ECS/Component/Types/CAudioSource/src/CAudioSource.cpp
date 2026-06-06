#include "Engine/Systems/ECS/Component/Types/CAudioSource/include/CAudioSource.h"

#include "Engine/Core/Globals.h"                 // down_cast
#include "Engine/Core/FileSystem/FileSystem.h"   // GetFilename
#include "Engine/Modules/ModuleAudio/include/ModuleAudio.h"
#include "Engine/Modules/ModuleScene/include/ModuleScene.h"
#include "Engine/Modules/ModuleResourceManager/include/ModuleResourceManager.h"
#include "Engine/Systems/ECS/Scene/include/Scene.h"
#include "Engine/Systems/ECS/GameObject/include/GameObject.h"
#include "Engine/Systems/ResourceManager/Core/ResourceBase/include/ResourceBase.h"
#include "Engine/Systems/ResourceManager/Types/ResourceAudio/include/ResourceAudio.h"
#include "Engine/Systems/ResourceManager/Types/ResourceType.h"
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

// ---------------------------------------------------------------------------
// Broker access
// ---------------------------------------------------------------------------

ModuleScene* CAudioSource::GetModuleScene() const
{
    auto go = GetGameObject();
    Scene* scene = go.IsValid() ? go.GetScene() : nullptr;
    return scene ? scene->GetModuleScene() : nullptr;
}

ModuleAudio* CAudioSource::GetAudioModule() const
{
    ModuleScene* moduleScene = GetModuleScene();
    return moduleScene ? moduleScene->GetAudio() : nullptr;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void CAudioSource::OnUpdate(float /*deltaTime*/)
{
    ModuleScene* moduleScene = GetModuleScene();
    ModuleAudio* audio = moduleScene ? moduleScene->GetAudio() : nullptr;
    if (!moduleScene || !audio)
        return;  // headless / test scene — no audio broker wired

    UpdatePreviewVoice(*moduleScene, *audio);
    UpdatePlaybackState(*moduleScene, *audio);
}

// Editor preview voice maintenance: drop it once the sim starts (the real voice
// takes over) or when it finishes, otherwise keep it in sync with live Inspector tweaks.
void CAudioSource::UpdatePreviewVoice(ModuleScene& moduleScene, ModuleAudio& audio)
{
    if (!m_previewSound)
        return;

    if (!moduleScene.IsStopped() || !audio.IsSoundPlaying(m_previewSound.Get()))
    {
        m_previewSound.Reset();
    }
    else
    {
        audio.SetSoundVolume(m_previewSound.Get(), volume);
        audio.SetSoundPitch(m_previewSound.Get(), pitch);
    }
}

// Play-driven voice state machine: tracks the scene's STOPPED/PAUSED/PLAYING edges
// and creates/starts/pauses/releases the backend voice accordingly.
void CAudioSource::UpdatePlaybackState(ModuleScene& moduleScene, ModuleAudio& audio)
{
    // STOPPED — tear the voice down so the next play session starts cleanly.
    if (moduleScene.IsStopped())
    {
        m_sound.Reset();
        m_started = false;
        m_paused  = false;
        return;
    }

    // PAUSED — stop the voice but retain its cursor (resumed on PLAYING).
    if (moduleScene.IsPaused())
    {
        if (m_sound && !m_paused)
        {
            audio.StopSound(m_sound.Get());
            m_paused = true;
        }
        return;
    }

    // PLAYING.
    if (!m_started)
    {
        // First frame of the play session: honour playOnAwake.
        if (playOnAwake && clip)
        {
            if (!m_sound)
                m_sound = AudioVoice(&audio, audio.CreateSound(clip));

            if (m_sound)
            {
                audio.SetSoundLooping(m_sound.Get(), loop);
                audio.StartSound(m_sound.Get());
            }
        }
        // Mark started regardless so we don't retry voice creation every frame
        // when there is no clip or playOnAwake is false.
        m_started = true;
    }
    else if (m_paused && m_sound)
    {
        // Resume from the retained cursor after a pause.
        audio.StartSound(m_sound.Get());
        m_paused = false;
    }

    // Push live parameters every frame so Inspector tweaks take effect immediately.
    if (m_sound)
    {
        audio.SetSoundVolume(m_sound.Get(), volume);
        audio.SetSoundPitch(m_sound.Get(), pitch);
    }
}

void CAudioSource::OnDestroy()
{
    auto go = GetGameObject();
    Scene* scene = go.IsValid() ? go.GetScene() : nullptr;

    // Release both voices now (on logical destroy) rather than waiting for the member
    // destructors — a component removed at runtime should stop playing immediately.
    // Each AudioVoice releases through its captured broker; safe because ModuleAudio
    // outlives the scene (see AudioVoice / Application module order).
    m_sound.Reset();
    m_previewSound.Reset();
    m_started = false;
    m_paused  = false;

    // Drop our reference to the clip resource (mirrors CMesh::OnDestroy).
    if (clip && scene && scene->GetResourceManager())
        scene->GetResourceManager()->UnloadResource(clip->GetUID());
    clip = nullptr;
}

// ---------------------------------------------------------------------------
// Editor preview (edit-mode auditioning)
// ---------------------------------------------------------------------------

void CAudioSource::PreviewPlay()
{
    ModuleAudio* audio = GetAudioModule();
    if (!audio || !clip)
        return;

    // One preview at a time — the move-assign releases any voice still in flight.
    m_previewSound = AudioVoice(audio, audio->CreateSound(clip));
    if (m_previewSound)
    {
        // Preview is a one-shot audition (never loops) so it cleans up on its own
        // even if the Inspector is closed mid-playback. Volume/pitch follow the
        // current authored values and stay live via OnUpdate.
        audio->SetSoundLooping(m_previewSound.Get(), false);
        audio->SetSoundVolume(m_previewSound.Get(), volume);
        audio->SetSoundPitch(m_previewSound.Get(), pitch);
        audio->StartSound(m_previewSound.Get());
    }
}

void CAudioSource::PreviewStop()
{
    // AudioVoice releases through its captured broker — no need to re-resolve it.
    m_previewSound.Reset();
}

bool CAudioSource::IsPreviewPlaying() const
{
    ModuleAudio* audio = GetAudioModule();
    return audio && m_previewSound && audio->IsSoundPlaying(m_previewSound.Get());
}

bool CAudioSource::HasActiveVoice() const
{
    return static_cast<bool>(m_sound);
}

double CAudioSource::GetPlaybackSeconds() const
{
    ModuleAudio* audio = GetAudioModule();
    if (!audio || !m_sound)
        return 0.0;
    return audio->GetCursorSeconds(m_sound.Get());
}

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

JsonObject CAudioSource::Serialize() const
{
    JsonObject root;
    root.Set("type",      GetType());

    // Store both asset and library paths + UID so GameApp can resolve the clip
    // without .meta files, exactly like CMesh.
    root.Set("assetPath", clip ? clip->GetAssetsPath() : "");
    if (clip)
    {
        root.Set("libraryPath", clip->GetLibraryPath());
        root.Set("resourceUID", static_cast<double>(clip->GetUID()));
    }

    root.Set("volume",      volume);
    root.Set("pitch",       pitch);
    root.Set("loop",        loop);
    root.Set("playOnAwake", playOnAwake);
    return root;
}

void CAudioSource::Deserialize(const JsonObject& obj)
{
    volume      = obj.GetFloat("volume",      volume);
    pitch       = obj.GetFloat("pitch",       pitch);
    loop        = obj.GetBool ("loop",        loop);
    playOnAwake = obj.GetBool ("playOnAwake", playOnAwake);

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
                resourceUID, ResourceType::AUDIO,
                nous::engine::filesystem::GetFilename(assetPath), assetPath, libraryPath))
            clip = down_cast<ResourceAudio*>(r);
    }

    // EDITOR path / fallback: resolve via the asset path.
    if (!clip && !assetPath.empty())
    {
        if (ResourceBase* r = rm->CreateResource(assetPath))
            clip = down_cast<ResourceAudio*>(r);
    }
}
