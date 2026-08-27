#include <ECS/Component/Types/CAudioSource/CAudioSource.h>

#include "Engine/Core/Globals.h"                 // down_cast
#include <FileSystem/FileSystem.h>   // GetFilename
#include <ECS/ComponentServices.h>
#include <ECS/Scene/iSceneHost.h>
#include <AudioSystem/iAudioBroker.h>
#include <ResourceManager/Core/IResourceLoader.h>
#include <ECS/GameObject.h>
#include <ECS/Component/Types/CTransform/CTransform.h>
#include <ECS/Component/Types/CVideoPlayer/CVideoPlayer.h>
#include <ResourceManager/Core/ResourceBase.h>
#include <ResourceManager/Types/ResourceAudio/ResourceAudio.h>
#include <ResourceManager/Types/ResourceAudioGraph/ResourceAudioGraph.h>
#include <ResourceManager/Types/ResourceType.h>
#include "Engine/Utils/Serialization/JsonFile/JsonObject.h"

#include <cmath>

namespace
{
    const char* AttenuationToString(AttenuationModel m)
    {
        switch (m)
        {
            case AttenuationModel::None:        return "None";
            case AttenuationModel::Linear:      return "Linear";
            case AttenuationModel::Exponential: return "Exponential";
            case AttenuationModel::Inverse:     // fallthrough
            default:                            return "Inverse";
        }
    }

    AttenuationModel AttenuationFromString(const std::string& s)
    {
        if (s == "None")        return AttenuationModel::None;
        if (s == "Linear")      return AttenuationModel::Linear;
        if (s == "Exponential") return AttenuationModel::Exponential;
        return AttenuationModel::Inverse;
    }

    const char* BusToString(AudioBus b)
    {
        switch (b)
        {
            case AudioBus::Master:  return "Master";
            case AudioBus::Music:   return "Music";
            case AudioBus::UI:      return "UI";
            case AudioBus::Ambient: return "Ambient";
            case AudioBus::SFX:     // fallthrough — default
            default:                return "SFX";
        }
    }

    AudioBus BusFromString(const std::string& s)
    {
        if (s == "Master")  return AudioBus::Master;
        if (s == "Music")   return AudioBus::Music;
        if (s == "UI")      return AudioBus::UI;
        if (s == "Ambient") return AudioBus::Ambient;
        return AudioBus::SFX;
    }
}

// ---------------------------------------------------------------------------
// Effect chain helpers
// ---------------------------------------------------------------------------

void CAudioSource::BuildChain(const IAudioBroker& audio, const AudioVoice& voice, EffectChainHandle& outChain)
{
    if (outChain || !voice)                                   // already built, or no voice
        return;
    if (!effectGraph || effectGraph->effects.empty())         // dry — no chain
        return;

    outChain = audio.CreateEffectChain(voice.Get(), effectGraph->effects, targetBus);
    m_chainGeneration = effectGraph->generation;
}

void CAudioSource::DestroyChain(const IAudioBroker& audio, EffectChainHandle& chain)
{
    if (chain)
    {
        audio.DestroyEffectChain(chain);
        chain = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void CAudioSource::OnUpdate(float /*deltaTime*/)
{
    const ComponentServices& s = Services();
    if (!s.host || !s.audio)
        return;  // headless / test scene — no audio broker wired

    UpdatePreviewVoice(*s.host, *s.audio);
    UpdatePlaybackState(*s.host, *s.audio);
}

// Editor preview voice maintenance: drop it once the sim starts (the real voice
// takes over) or when it finishes, otherwise keep it in sync with live Inspector tweaks.
void CAudioSource::UpdatePreviewVoice(const ISceneHost& host, const IAudioBroker& audio)
{
    if (!m_previewSound)
        return;

    if (!host.IsStopped() || !audio.IsSoundPlaying(m_previewSound.Get()))
    {
        DestroyChain(audio, m_previewChain);   // before the voice it hangs off
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
void CAudioSource::UpdatePlaybackState(const ISceneHost& host, const IAudioBroker& audio)
{
    // STOPPED — tear the voice down so the next play session starts cleanly.
    if (host.IsStopped())
    {
        DestroyChain(audio, m_chain);   // before m_sound.Reset() — chain reattaches the voice to its bus
        m_sound.Reset();
        m_started = false;
        m_paused  = false;
        return;
    }

    // PAUSED — stop the voice but retain its cursor (resumed on PLAYING).
    if (host.IsPaused())
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
        // If a sibling video player is still loading (e.g. predecoding all its frames),
        // hold our start until it's ready so the audio and video begin together instead of
        // the music racing ahead during the decode hitch. Retried next frame; once the video
        // finishes its load attempt (success or failure) the gate opens.
        auto go = GetGameObject();
        if (CVideoPlayer* video = go.IsValid() ? go.TryGetComponent<CVideoPlayer>() : nullptr)
        {
            if (video->IsLoadingForPlayback())
                return;
        }

        // First frame of the play session: honour playOnAwake.
        if (playOnAwake && clip)
        {
            if (!m_sound)
                m_sound = AudioVoice(&audio, audio.CreateSound(clip, targetBus));

            if (m_sound)
            {
                BuildChain(audio, m_sound, m_chain);   // splice effects between the voice and its bus
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

        // Spatialization is a live toggle (like volume/pitch). When on, push the
        // GO's position + distance attenuation; the main CAudioListener supplies
        // the listener pose. Default-off voices stay 2D (set in CreateSound).
        audio.SetSoundSpatializationEnabled(m_sound.Get(), spatialize);
        if (spatialize)
        {
            auto go = GetGameObject();
            if (auto* t = go.IsValid() ? go.TryGetComponent<CTransform>() : nullptr)
                audio.SetSoundPosition(m_sound.Get(), t->position.x, t->position.y, t->position.z);

            audio.SetSoundMinDistance(m_sound.Get(), minDistance);
            audio.SetSoundMaxDistance(m_sound.Get(), maxDistance);
            audio.SetSoundAttenuationModel(m_sound.Get(), attenuation);
        }

        // Effect-graph live rebuild: the editor bumps generation on save (Layer 3),
        // so this is dormant in Layer 2. Also covers the first-build case where a
        // graph is assigned after the voice already exists.
        if (effectGraph && m_chain && effectGraph->generation != m_chainGeneration)
        {
            DestroyChain(audio, m_chain);
            BuildChain(audio, m_sound, m_chain);
        }
        else if (effectGraph && !m_chain && !effectGraph->effects.empty())
        {
            BuildChain(audio, m_sound, m_chain);
        }
    }
}

void CAudioSource::OnDestroy()
{
    const ComponentServices& s = Services();

    // Destroy chains before the voices they hang off (chain reattaches the voice to
    // its bus on teardown). ModuleAudio outlives the scene, so the broker resolves.
    if (const IAudioBroker* audio = s.audio)
    {
        DestroyChain(*audio, m_chain);
        DestroyChain(*audio, m_previewChain);
    }

    // Release both voices now (on logical destroy) rather than waiting for the member
    // destructors — a component removed at runtime should stop playing immediately.
    // Each AudioVoice releases through its captured broker; safe because ModuleAudio
    // outlives the scene (see AudioVoice / Application module order).
    m_sound.Reset();
    m_previewSound.Reset();
    m_started = false;
    m_paused  = false;

    // Drop our reference to the clip resource (mirrors CMesh::OnDestroy).
    if (clip && s.resources)
        s.resources->UnloadResource(clip->GetUID());
    clip = nullptr;

    // Drop our reference to the effect-graph resource (mirrors the clip).
    if (effectGraph && s.resources)
        s.resources->UnloadResource(effectGraph->GetUID());
    effectGraph = nullptr;
}

// ---------------------------------------------------------------------------
// Editor preview (edit-mode auditioning)
// ---------------------------------------------------------------------------

void CAudioSource::PreviewPlay()
{
    const IAudioBroker* audio = Services().audio;
    if (!audio || !clip)
        return;

    // One preview at a time — the move-assign releases any voice still in flight.
    m_previewSound = AudioVoice(audio, audio->CreateSound(clip, targetBus));
    if (m_previewSound)
    {
        // Preview is a one-shot audition (never loops) so it cleans up on its own
        // even if the Inspector is closed mid-playback. Volume/pitch follow the
        // current authored values and stay live via OnUpdate.
        audio->SetSoundLooping(m_previewSound.Get(), false);
        audio->SetSoundSpatializationEnabled(m_previewSound.Get(), false);  // preview is 2D
        BuildChain(*audio, m_previewSound, m_previewChain);                  // audition WITH effects
        audio->SetSoundVolume(m_previewSound.Get(), volume);
        audio->SetSoundPitch(m_previewSound.Get(), pitch);
        audio->StartSound(m_previewSound.Get());
    }
}

void CAudioSource::PreviewStop()
{
    // Destroy the chain before the voice it hangs off (chain reattaches the voice).
    if (const IAudioBroker* audio = Services().audio)
        DestroyChain(*audio, m_previewChain);

    // AudioVoice releases through its captured broker — no need to re-resolve it.
    m_previewSound.Reset();
}

bool CAudioSource::IsPreviewPlaying() const
{
    const IAudioBroker* audio = Services().audio;
    return audio && m_previewSound && audio->IsSoundPlaying(m_previewSound.Get());
}

bool CAudioSource::IsVoicePlaying() const
{
    const IAudioBroker* audio = Services().audio;
    return audio && m_sound && audio->IsSoundPlaying(m_sound.Get());
}

double CAudioSource::GetPlaybackSeconds() const
{
    const IAudioBroker* audio = Services().audio;
    if (!audio || !m_sound)
        return 0.0;

    const double cursor = audio->GetCursorSeconds(m_sound.Get());

    // miniaudio's cursor grows monotonically across loop iterations — it does NOT
    // reset to 0 when a looping sound wraps. Fold it into the clip's duration so we
    // report a loop-relative position (the master clock a synced video tracks).
    // Without this, once the cursor exceeds a sibling video's duration every frame
    // satisfies pts <= cursor and the video races at decode speed. No-op on the
    // first pass / non-looping clips (cursor < duration).
    const double duration = clip ? static_cast<double>(clip->GetDurationSec()) : 0.0;
    if (duration > 0.0 && cursor >= duration)
        return std::fmod(cursor, duration);

    return cursor;
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

    // Effect graph reference (same asset+library+UID pattern as the clip).
    root.Set("effectGraphAssetPath", effectGraph ? effectGraph->GetAssetsPath() : "");
    if (effectGraph)
    {
        root.Set("effectGraphLibraryPath", effectGraph->GetLibraryPath());
        root.Set("effectGraphUID",         static_cast<double>(effectGraph->GetUID()));
    }

    root.Set("volume",      volume);
    root.Set("pitch",       pitch);
    root.Set("loop",        loop);
    root.Set("playOnAwake", playOnAwake);

    root.Set("spatialize",  spatialize);
    root.Set("minDistance", minDistance);
    root.Set("maxDistance", maxDistance);
    root.Set("attenuation", AttenuationToString(attenuation));
    root.Set("targetBus",   BusToString(targetBus));
    return root;
}

void CAudioSource::Deserialize(const JsonObject& obj)
{
    volume      = obj.GetFloat("volume",      volume);
    pitch       = obj.GetFloat("pitch",       pitch);
    loop        = obj.GetBool ("loop",        loop);
    playOnAwake = obj.GetBool ("playOnAwake", playOnAwake);

    spatialize  = obj.GetBool ("spatialize",  spatialize);
    minDistance = obj.GetFloat("minDistance", minDistance);
    maxDistance = obj.GetFloat("maxDistance", maxDistance);
    attenuation = AttenuationFromString(obj.GetString("attenuation", AttenuationToString(attenuation)));
    targetBus   = BusFromString(obj.GetString("targetBus", BusToString(targetBus)));

    IResourceLoader* rm = Services().resources;
    if (!rm)
        return;

    // ── Clip ──
    const std::string assetPath   = obj.GetString("assetPath");
    const std::string libraryPath = obj.GetString("libraryPath");
    const uint32      resourceUID = static_cast<uint32>(obj.GetDouble("resourceUID", 0.0));

    if (!assetPath.empty() || !libraryPath.empty())
    {
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

    // ── Effect graph (resolved the same way; a source may have a graph and no clip) ──
    const std::string fxAssetPath   = obj.GetString("effectGraphAssetPath");
    const std::string fxLibraryPath = obj.GetString("effectGraphLibraryPath");
    const uint32      fxUID         = static_cast<uint32>(obj.GetDouble("effectGraphUID", 0.0));

    if (!fxAssetPath.empty() || !fxLibraryPath.empty())
    {
        if (!fxLibraryPath.empty() && fxUID != 0)
        {
            if (ResourceBase* r = rm->CreateResourceFromLibrary(
                    fxUID, ResourceType::AUDIO_GRAPH,
                    nous::engine::filesystem::GetFilename(fxAssetPath), fxAssetPath, fxLibraryPath))
                effectGraph = down_cast<ResourceAudioGraph*>(r);
        }
        if (!effectGraph && !fxAssetPath.empty())
        {
            if (ResourceBase* r = rm->CreateResource(fxAssetPath))
                effectGraph = down_cast<ResourceAudioGraph*>(r);
        }
    }
}
