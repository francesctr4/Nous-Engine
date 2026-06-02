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

ModuleAudio* CAudioSource::GetAudioModule() const
{
    auto go = GetGameObject();
    Scene* scene = go.IsValid() ? go.GetScene() : nullptr;
    ModuleScene* moduleScene = scene ? scene->GetModuleScene() : nullptr;
    return moduleScene ? moduleScene->GetAudio() : nullptr;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void CAudioSource::OnUpdate(float /*deltaTime*/)
{
    auto go = GetGameObject();
    if (!go.IsValid())
        return;

    Scene* scene = go.GetScene();
    ModuleScene* moduleScene = scene ? scene->GetModuleScene() : nullptr;
    ModuleAudio* audio = moduleScene ? moduleScene->GetAudio() : nullptr;
    if (!audio)
        return;  // headless / test scene — no audio broker wired

    // Editor preview voice maintenance: drop it once the sim starts (the real
    // voice takes over) or when it finishes, otherwise keep it in sync with
    // live Inspector tweaks.
    if (m_previewSound)
    {
        if (!moduleScene->IsStopped() || !audio->IsSoundPlaying(m_previewSound))
        {
            audio->DestroySound(m_previewSound);
            m_previewSound = nullptr;
        }
        else
        {
            audio->SetSoundVolume(m_previewSound, volume);
            audio->SetSoundPitch(m_previewSound, pitch);
        }
    }

    // STOPPED — tear the voice down so the next play session starts cleanly.
    if (moduleScene->IsStopped())
    {
        if (m_sound)
        {
            audio->DestroySound(m_sound);
            m_sound = nullptr;
        }
        m_started = false;
        m_paused  = false;
        return;
    }

    // PAUSED — stop the voice but retain its cursor (resumed on PLAYING).
    if (moduleScene->IsPaused())
    {
        if (m_sound && !m_paused)
        {
            audio->StopSound(m_sound);
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
                m_sound = audio->CreateSound(clip);

            if (m_sound)
            {
                audio->SetSoundLooping(m_sound, loop);
                audio->StartSound(m_sound);
            }
        }
        // Mark started regardless so we don't retry voice creation every frame
        // when there is no clip or playOnAwake is false.
        m_started = true;
    }
    else if (m_paused && m_sound)
    {
        // Resume from the retained cursor after a pause.
        audio->StartSound(m_sound);
        m_paused = false;
    }

    // Push live parameters every frame so Inspector tweaks take effect immediately.
    if (m_sound)
    {
        audio->SetSoundVolume(m_sound, volume);
        audio->SetSoundPitch(m_sound, pitch);
    }
}

void CAudioSource::OnDestroy()
{
    auto go = GetGameObject();
    Scene* scene = go.IsValid() ? go.GetScene() : nullptr;
    ModuleScene* moduleScene = scene ? scene->GetModuleScene() : nullptr;
    ModuleAudio* audio = moduleScene ? moduleScene->GetAudio() : nullptr;

    // Release both voices. The audio module is constructed before the scene, so it
    // is guaranteed alive during scene teardown (see Application module order).
    if (audio)
    {
        if (m_sound)        audio->DestroySound(m_sound);
        if (m_previewSound) audio->DestroySound(m_previewSound);
    }
    m_sound        = nullptr;
    m_previewSound = nullptr;
    m_started      = false;
    m_paused       = false;

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

    // One preview at a time — replace any voice still in flight.
    if (m_previewSound)
        audio->DestroySound(m_previewSound);

    m_previewSound = audio->CreateSound(clip);
    if (m_previewSound)
    {
        // Preview is a one-shot audition (never loops) so it cleans up on its own
        // even if the Inspector is closed mid-playback. Volume/pitch follow the
        // current authored values and stay live via OnUpdate.
        audio->SetSoundLooping(m_previewSound, false);
        audio->SetSoundVolume(m_previewSound, volume);
        audio->SetSoundPitch(m_previewSound, pitch);
        audio->StartSound(m_previewSound);
    }
}

void CAudioSource::PreviewStop()
{
    if (ModuleAudio* audio = GetAudioModule(); audio && m_previewSound)
        audio->DestroySound(m_previewSound);
    m_previewSound = nullptr;
}

bool CAudioSource::IsPreviewPlaying() const
{
    ModuleAudio* audio = GetAudioModule();
    return audio && m_previewSound && audio->IsSoundPlaying(m_previewSound);
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
