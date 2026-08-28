#pragma once

#include <EngineCore/EngineExport.h>
#include <string>
#include <cstdint>

// ---------------------------------------------------------------------------
// Decode-only audio probe — asset-import API, not runtime playback.
//
// Stateless and thread-safe: opens an independent ma_decoder, reads metadata,
// closes it. Implemented in AudioProbe.cpp (header-only miniaudio include; the
// decoder symbols link from MiniaudioBackend.cpp's MA_IMPLEMENTATION). Lives
// outside IAudioEngineBackend because probing is unrelated to playback — the
// importer calls this directly, the way ImporterTexture calls stb_image and
// ImporterVideo calls ProbeVideoFile.
// ---------------------------------------------------------------------------

struct AudioProbeInfo
{
    float  durationSec  = 0.0f;
    uint32_t sampleRate   = 0;
    uint8_t  channelCount = 0;
};

NOUS_ENGINE_API bool ProbeAudioFile(const std::string& libraryPath, AudioProbeInfo& outInfo);
