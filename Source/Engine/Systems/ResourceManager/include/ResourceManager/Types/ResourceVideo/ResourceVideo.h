#pragma once

#include <EngineCore/Globals.h>
#include <EngineCore/EngineExport.h>
#include <ResourceManager/Core/ResourceBase.h>

#include <string>

enum class VideoFileType : uint8_t
{
    UNKNOWN,
    MP4,
    GIF
};

enum class VideoDecodeMode : uint8_t
{
    STREAMED,    // live decoder thread + ring buffer (long video, e.g. .mp4)
    PREDECODED   // all frames decoded into memory once, no thread (small loopable, e.g. .gif)
};

// ---------------------------------------------------------------------------
// Extension-driven import policy (parallels ImporterAudio's wav/ogg mapping).
// Exposed as free functions so the policy is unit-testable without filesystem I/O.
//   .mp4 -> MP4 / STREAMED      (long, compressed: stream from disk)
//   .gif -> GIF / PREDECODED    (small, loopable: decode to memory, cycle)
//   unknown -> UNKNOWN / STREAMED (safe default)
// ---------------------------------------------------------------------------
NOUS_ENGINE_API VideoFileType   VideoFileTypeFromExtension(const std::string& libraryPath);
NOUS_ENGINE_API VideoDecodeMode VideoDecodeModeFromFileType(VideoFileType fileType);

class ResourceVideo : public ResourceBase
{
public:
    NOUS_ENGINE_API explicit ResourceVideo(uint32 uid = 0);
    NOUS_ENGINE_API ~ResourceVideo() override;

    NOUS_ENGINE_API void SetFileType(VideoFileType _fileType);
    NOUS_ENGINE_API void SetDecodeMode(VideoDecodeMode _decodeMode);
    NOUS_ENGINE_API void SetWidth(uint32 _width);
    NOUS_ENGINE_API void SetHeight(uint32 _height);
    NOUS_ENGINE_API void SetDurationSec(float _durationSec);
    NOUS_ENGINE_API void SetFrameRate(float _frameRate);
    NOUS_ENGINE_API void SetCodecName(std::string_view _codecName);
    NOUS_ENGINE_API void SetHasAudioTrack(bool _hasAudioTrack);

    [[nodiscard]] NOUS_ENGINE_API VideoFileType   GetFileType() const;
    [[nodiscard]] NOUS_ENGINE_API VideoDecodeMode GetDecodeMode() const;
    [[nodiscard]] NOUS_ENGINE_API uint32          GetWidth() const;
    [[nodiscard]] NOUS_ENGINE_API uint32          GetHeight() const;
    [[nodiscard]] NOUS_ENGINE_API float           GetDurationSec() const;
    [[nodiscard]] NOUS_ENGINE_API float           GetFrameRate() const;
    [[nodiscard]] NOUS_ENGINE_API std::string     GetCodecName() const;
    [[nodiscard]] NOUS_ENGINE_API bool            GetHasAudioTrack() const;

private:
    // Set by ImporterVideo from the extension at import time.
    VideoFileType   fileType;
    VideoDecodeMode decodeMode;

    // Probed once by ImporterVideo after the library copy is in place. Read by
    // the Inspector / Assets Browser only; never touched by the (future) decode thread.
    uint32      width;
    uint32      height;
    float       durationSec;
    float       frameRate;   // average fps (approximate for GIF)
    std::string codecName;
    bool        hasAudioTrack;  // for the future cutscene/audio phase
};
