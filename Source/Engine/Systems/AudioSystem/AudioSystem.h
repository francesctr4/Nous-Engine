#pragma once

#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"
#include "Engine/Systems/AudioSystem/SoundHandle.h"

#include <cstdint>
#include <string>

enum class AudioEngineBackend : std::int8_t
{
    UNKNOWN = -1,
    MINIAUDIO = 0
};

class ResourceAudio;
class IAudioEngineBackend;

class AudioSystem
{
public:

    AudioSystem();

    bool Initialize(AudioEngineBackend backend);
    void PlayAudio(ResourceAudio* rAudio) const;
    void Shutdown();

    // Per-voice sound lifecycle — forwarded to the active backend. All calls
    // safely no-op (or return null/false) when no backend is initialized.
    SoundHandle CreateSound(ResourceAudio* rAudio) const;
    void        DestroySound(SoundHandle sound) const noexcept;

    void        StartSound(SoundHandle sound) const;
    void        StopSound(SoundHandle sound) const;

    void        SetSoundVolume(SoundHandle sound, float volume) const;
    void        SetSoundPitch(SoundHandle sound, float pitch) const;
    void        SetSoundLooping(SoundHandle sound, bool looping) const;

    bool        IsSoundPlaying(SoundHandle sound) const;

private:
    IAudioEngineBackend* m_audioEngine;
};

// ---------------------------------------------------------------------------
// Decode-only audio probe — asset-import API, not a runtime-playback one.
//
// Stateless and thread-safe: uses an independent decoder, no shared engine
// state. Implemented by whichever audio backend the build links in (currently
// MiniaudioBackend.cpp). Lives outside IAudioEngineBackend because probing is
// unrelated to playback — the importer calls this directly, the way
// ImporterTexture calls stb_image directly.
// ---------------------------------------------------------------------------

struct AudioProbeInfo
{
    float  durationSec  = 0.0f;
    uint32 sampleRate   = 0;
    uint8  channelCount = 0;
};

NOUS_ENGINE_API bool ProbeAudioFile(const std::string& libraryPath, AudioProbeInfo& outInfo);
