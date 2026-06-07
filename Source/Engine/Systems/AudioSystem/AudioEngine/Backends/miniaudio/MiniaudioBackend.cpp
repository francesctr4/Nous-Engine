#include "MiniaudioBackend.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Systems/AudioSystem/AudioTypes.h"
#include "Engine/Systems/ResourceManager/Types/ResourceAudio/include/ResourceAudio.h"

// miniaudio Library Documentation
// https://miniaud.io/docs/manual/index.html
//
// COM apartment (Windows): SDL's file drag-and-drop uses OLE (RegisterDragDrop),
// which requires the process's main thread to be a single-threaded apartment (STA).
// By default miniaudio initializes COM as COINIT_MULTITHREADED (MA_COINIT_VALUE == 0)
// on the thread that calls ma_engine_init — which is our main thread — clobbering
// SDL's apartment and silently disabling drag-and-drop onto the window. Forcing
// apartment-threaded (COINIT_APARTMENTTHREADED == 0x2) keeps miniaudio in lockstep
// with SDL so both can coexist. WASAPI runs fine under STA (its device worker
// creates/uses its own COM objects on its own thread).
#define MA_COINIT_VALUE 2  /* COINIT_APARTMENTTHREADED */

// Vorbis (.ogg) support: miniaudio has no built-in Vorbis decoder. It auto-detects
// stb_vorbis when stb_vorbis.c is included in the same TU as MA_IMPLEMENTATION,
// using this header-only / implementation sandwich.
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>
#define MA_IMPLEMENTATION
#include <miniaudio.h>
#undef STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_AUDIOSYSTEM;

MiniaudioBackend::MiniaudioBackend() : m_audioEngine({})
{

}

MiniaudioBackend::~MiniaudioBackend()
{

}

bool MiniaudioBackend::Initialize()
{
    const ma_result result = ma_engine_init(nullptr, &m_audioEngine);

    if (result != MA_SUCCESS)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to initialize audio engine. Error code: %d",
            static_cast<int>(result));
        return false;
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "miniaudio engine initialized successfully.");

    if (!m_busGraph.Initialize(&m_audioEngine))
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to initialize audio bus graph.");
        ma_engine_uninit(&m_audioEngine);
        return false;
    }

    return true;
}

void MiniaudioBackend::PlayAudio(ResourceAudio* rAudio)
{
    const ma_result result =
        ma_engine_play_sound(&m_audioEngine, rAudio->GetLibraryPath().c_str(), nullptr);

    if (result != MA_SUCCESS)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to play audio '%s'. Error code: %d",
            rAudio->GetLibraryPath().c_str(), static_cast<int>(result));
        return;
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "Successfully playing audio: '%s' from '%s'",
        rAudio->GetName().c_str(), rAudio->GetLibraryPath().c_str());
}

// ---------------------------------------------------------------------------
// Per-voice sound lifecycle
// ---------------------------------------------------------------------------

namespace
{
    // SoundHandle is an opaque void* over a heap-allocated ma_sound.
    ma_sound* AsSound(SoundHandle sound)
    {
        return static_cast<ma_sound*>(sound);
    }
}

SoundHandle MiniaudioBackend::CreateSound(ResourceAudio* rAudio, AudioBus bus)
{
    if (!rAudio)
        return nullptr;

    // Spatialization-capable voice: do NOT bake MA_SOUND_FLAG_NO_SPATIALIZATION.
    // We disable spatialization right after init so the default is still 2D
    // (identical to the old behavior), but CAudioSource can toggle it at runtime.
    // STREAMED clips read from disk on demand; DECODED clips are fully decoded
    // into memory up front (short SFX).
    ma_uint32 flags = (rAudio->GetStreamingMode() == AudioStreamingMode::STREAMED)
        ? MA_SOUND_FLAG_STREAM
        : MA_SOUND_FLAG_DECODE;

    ma_sound* sound = NOUS_NEW<ma_sound>(MemoryTag::AUDIO_SYSTEM);

    const ma_result result = ma_sound_init_from_file(
        &m_audioEngine, rAudio->GetLibraryPath().c_str(), flags,
        m_busGraph.GetGroup(bus), nullptr, sound);

    if (result != MA_SUCCESS)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create sound from '%s'. Error code: %d",
            rAudio->GetLibraryPath().c_str(), static_cast<int>(result));
        NOUS_DELETE(sound, MemoryTag::AUDIO_SYSTEM);
        return nullptr;
    }

    // Default to 2D; CAudioSource enables spatialization on spatial sources.
    ma_sound_set_spatialization_enabled(sound, MA_FALSE);

    return sound;
}

void MiniaudioBackend::DestroySound(SoundHandle sound) noexcept
{
    if (!sound)
        return;

    ma_sound* s = AsSound(sound);
    ma_sound_uninit(s);
    NOUS_DELETE(s, MemoryTag::AUDIO_SYSTEM);
}

void MiniaudioBackend::StartSound(SoundHandle sound)
{
    if (sound)
        ma_sound_start(AsSound(sound));
}

void MiniaudioBackend::StopSound(SoundHandle sound)
{
    if (sound)
        ma_sound_stop(AsSound(sound));
}

void MiniaudioBackend::SetSoundVolume(SoundHandle sound, float volume)
{
    if (sound)
        ma_sound_set_volume(AsSound(sound), volume);
}

void MiniaudioBackend::SetSoundPitch(SoundHandle sound, float pitch)
{
    if (sound)
        ma_sound_set_pitch(AsSound(sound), pitch);
}

void MiniaudioBackend::SetSoundLooping(SoundHandle sound, bool looping)
{
    if (sound)
        ma_sound_set_looping(AsSound(sound), looping ? MA_TRUE : MA_FALSE);
}

bool MiniaudioBackend::IsSoundPlaying(SoundHandle sound) const
{
    return sound && ma_sound_is_playing(AsSound(sound)) == MA_TRUE;
}

double MiniaudioBackend::GetCursorSeconds(SoundHandle sound) const
{
    if (!sound)
        return 0.0;

    float cursor = 0.0f;
    ma_sound_get_cursor_in_seconds(AsSound(sound), &cursor);
    return static_cast<double>(cursor);
}

// ---------------------------------------------------------------------------
// 3D spatialization
// ---------------------------------------------------------------------------

namespace
{
    ma_attenuation_model ToMaAttenuation(AttenuationModel model)
    {
        switch (model)
        {
            case AttenuationModel::None:        return ma_attenuation_model_none;
            case AttenuationModel::Linear:      return ma_attenuation_model_linear;
            case AttenuationModel::Exponential: return ma_attenuation_model_exponential;
            case AttenuationModel::Inverse:     // fallthrough — default
            default:                            return ma_attenuation_model_inverse;
        }
    }
}

void MiniaudioBackend::SetListenerPosition(float x, float y, float z)
{
    ma_engine_listener_set_position(&m_audioEngine, 0, x, y, z);
}

void MiniaudioBackend::SetListenerDirection(float x, float y, float z)
{
    ma_engine_listener_set_direction(&m_audioEngine, 0, x, y, z);
}

void MiniaudioBackend::SetListenerWorldUp(float x, float y, float z)
{
    ma_engine_listener_set_world_up(&m_audioEngine, 0, x, y, z);
}

void MiniaudioBackend::SetSoundSpatializationEnabled(SoundHandle sound, bool enabled)
{
    if (sound)
        ma_sound_set_spatialization_enabled(AsSound(sound), enabled ? MA_TRUE : MA_FALSE);
}

void MiniaudioBackend::SetSoundPosition(SoundHandle sound, float x, float y, float z)
{
    if (sound)
        ma_sound_set_position(AsSound(sound), x, y, z);
}

void MiniaudioBackend::SetSoundMinDistance(SoundHandle sound, float distance)
{
    if (sound)
        ma_sound_set_min_distance(AsSound(sound), distance);
}

void MiniaudioBackend::SetSoundMaxDistance(SoundHandle sound, float distance)
{
    if (sound)
        ma_sound_set_max_distance(AsSound(sound), distance);
}

void MiniaudioBackend::SetSoundAttenuationModel(SoundHandle sound, AttenuationModel model)
{
    if (sound)
        ma_sound_set_attenuation_model(AsSound(sound), ToMaAttenuation(model));
}

// ---------------------------------------------------------------------------
// Bus mixer (forwarded to the owned MiniaudioBusGraph)
// ---------------------------------------------------------------------------

void  MiniaudioBackend::SetBusVolume(AudioBus bus, float volume) { m_busGraph.SetBusVolume(bus, volume); }
void  MiniaudioBackend::SetBusMute  (AudioBus bus, bool mute)    { m_busGraph.SetBusMute(bus, mute); }
void  MiniaudioBackend::SetBusSolo  (AudioBus bus, bool solo)    { m_busGraph.SetBusSolo(bus, solo); }
float MiniaudioBackend::GetBusVolume(AudioBus bus) const         { return m_busGraph.GetBusVolume(bus); }
bool  MiniaudioBackend::GetBusMute  (AudioBus bus) const         { return m_busGraph.GetBusMute(bus); }
bool  MiniaudioBackend::GetBusSolo  (AudioBus bus) const         { return m_busGraph.GetBusSolo(bus); }

// ---------------------------------------------------------------------------
// Effect chain
// ---------------------------------------------------------------------------

namespace
{
    MiniaudioEffectChain* AsChain(EffectChainHandle chain)
    {
        return static_cast<MiniaudioEffectChain*>(chain);
    }
}

EffectChainHandle MiniaudioBackend::CreateEffectChain(SoundHandle sound, const AudioGraphDesc& desc, AudioBus bus)
{
    if (!sound || desc.empty())
        return nullptr;

    // Chain output = the voice's bus group, or the engine endpoint for Master / before
    // the groups exist. (The group still applies bus volume/mute/solo after the chain.)
    ma_sound_group* group  = m_busGraph.GetGroup(bus);
    ma_node*        output = group ? reinterpret_cast<ma_node*>(group)
                                   : ma_engine_get_endpoint(&m_audioEngine);

    auto* chain = NOUS_NEW<MiniaudioEffectChain>(MemoryTag::AUDIO_SYSTEM);
    if (!chain->Build(&m_audioEngine, reinterpret_cast<ma_node*>(AsSound(sound)), desc, output))
    {
        NOUS_DELETE(chain, MemoryTag::AUDIO_SYSTEM);
        return nullptr;
    }
    return chain;
}

void MiniaudioBackend::SetEffectParam(EffectChainHandle chain, int effectIndex, int paramIndex, float value)
{
    if (chain)
        AsChain(chain)->SetParam(effectIndex, paramIndex, value);
}

void MiniaudioBackend::DestroyEffectChain(EffectChainHandle chain) noexcept
{
    if (!chain)
        return;
    MiniaudioEffectChain* c = AsChain(chain);
    c->Destroy();
    NOUS_DELETE(c, MemoryTag::AUDIO_SYSTEM);
}

void MiniaudioBackend::Shutdown() noexcept
{
    m_busGraph.Shutdown();      // uninit groups before the engine they hang off
    ma_engine_uninit(&m_audioEngine);

    NOUS_INFO_C(CURRENT_CHANNEL, "Audio engine shutdown successfully.");
}
