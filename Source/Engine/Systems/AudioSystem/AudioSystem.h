#pragma once

#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"

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
