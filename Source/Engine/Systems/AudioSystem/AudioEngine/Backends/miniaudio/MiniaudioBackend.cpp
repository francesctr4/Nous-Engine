#include "MiniaudioBackend.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"
#include "Engine/Systems/AudioSystem/AudioSystem.h"
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
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to initialize audio engine. Error code: {}", result);
        return false;
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "miniaudio engine initialized successfully.");
    return true;
}

void MiniaudioBackend::PlayAudio(ResourceAudio* rAudio)
{
    const ma_result result =
        ma_engine_play_sound(&m_audioEngine, rAudio->GetAssetsPath().c_str(), nullptr);

    if (result != MA_SUCCESS)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to play music. Error code: %s", result);
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "Successfully playing audio: '%s' from '%s'",
        rAudio->GetName().c_str(), rAudio->GetAssetsPath().c_str());
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

SoundHandle MiniaudioBackend::CreateSound(ResourceAudio* rAudio)
{
    if (!rAudio)
        return nullptr;

    // 2D source: skip spatialization entirely so volume/pitch behave predictably
    // regardless of listener position. STREAMED clips read from disk on demand;
    // DECODED clips are fully decoded into memory up front (short SFX).
    ma_uint32 flags = MA_SOUND_FLAG_NO_SPATIALIZATION;
    flags |= (rAudio->GetStreamingMode() == AudioStreamingMode::STREAMED)
        ? MA_SOUND_FLAG_STREAM
        : MA_SOUND_FLAG_DECODE;

    ma_sound* sound = NOUS_NEW<ma_sound>(MemoryTag::AUDIO_SYSTEM);

    const ma_result result = ma_sound_init_from_file(
        &m_audioEngine, rAudio->GetAssetsPath().c_str(), flags, nullptr, nullptr, sound);

    if (result != MA_SUCCESS)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create sound from '{}'. Error code: {}",
            rAudio->GetAssetsPath().c_str(), static_cast<int>(result));
        NOUS_DELETE(sound, MemoryTag::AUDIO_SYSTEM);
        return nullptr;
    }

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

bool ProbeAudioFile(const std::string& libraryPath, AudioProbeInfo& outInfo)
{
    ma_decoder decoder;
    const ma_result initResult = ma_decoder_init_file(libraryPath.c_str(), nullptr, &decoder);
    if (initResult != MA_SUCCESS)
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "ProbeAudioFile() failed to open '%s' (code %d)",
            libraryPath.c_str(), initResult);
        return false;
    }

    ma_uint64 frameCount = 0;
    const ma_result lenResult = ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount);

    outInfo.sampleRate   = static_cast<uint32>(decoder.outputSampleRate);
    outInfo.channelCount = static_cast<uint8>(decoder.outputChannels);
    outInfo.durationSec  = (lenResult == MA_SUCCESS && outInfo.sampleRate > 0)
        ? (static_cast<float>(frameCount) / static_cast<float>(outInfo.sampleRate))
        : 0.0f;

    ma_decoder_uninit(&decoder);
    return true;
}

void MiniaudioBackend::Shutdown() noexcept
{
    ma_engine_uninit(&m_audioEngine);

    NOUS_INFO_C(CURRENT_CHANNEL, "Audio engine shutdown successfully.");
}
