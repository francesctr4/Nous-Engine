#pragma once

#include <string>
#include <cstdint>

// ---------------------------------------------------------------------------
// Decode-only video probe — asset-import API, not runtime playback.
//
// Stateless and self-contained: opens an independent AVFormatContext, reads
// stream metadata, closes it. Implemented in VideoProbe.cpp, the only FFmpeg-
// including translation unit in Phase 1 (compile firewall). The importer calls
// this directly, the way ImporterTexture calls stb_image and ImporterAudio
// calls ProbeAudioFile.
// ---------------------------------------------------------------------------

struct VideoProbeInfo
{
    uint32_t      width         = 0;
    uint32_t      height        = 0;
    float       durationSec   = 0.0f;
    float       frameRate     = 0.0f;   // average fps (approximate for GIF)
    std::string codecName;
    bool        hasAudioTrack = false;
};

bool ProbeVideoFile(const std::string& libraryPath, VideoProbeInfo& outInfo);
