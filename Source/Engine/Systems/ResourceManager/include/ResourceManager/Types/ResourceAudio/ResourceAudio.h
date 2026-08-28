#pragma once

#include <ResourceManager/Core/ResourceBase.h>
#include <cstdint>

enum class AudioStreamingMode : uint8_t
{
    DECODED,   // ma_resource_manager decodes fully on first load (short SFX)
    STREAMED   // ma_resource_manager streams from disk (music / long ambient)
};

enum class AudioFileType : uint8_t
{
    UNKNOWN,
    WAV,
    OGG
};

class ResourceAudio : public ResourceBase
{
public:
    NOUS_ENGINE_API explicit ResourceAudio(uint32_t uid = 0);
    NOUS_ENGINE_API ~ResourceAudio() override;

    NOUS_ENGINE_API void SetStreamingMode(AudioStreamingMode _streamingMode);
    NOUS_ENGINE_API void SetFileType(AudioFileType _fileType);
    NOUS_ENGINE_API void SetDurationSec(float _durationSec);
    NOUS_ENGINE_API void SetSampleRate(uint32_t _sampleRate);
    NOUS_ENGINE_API void SetChannelCount(uint8_t _channelCount);
    NOUS_ENGINE_API void SetInternalData(void* _internalData);

    [[nodiscard]] NOUS_ENGINE_API AudioStreamingMode GetStreamingMode() const;
    [[nodiscard]] NOUS_ENGINE_API AudioFileType GetFileType() const;
    [[nodiscard]] NOUS_ENGINE_API float GetDurationSec() const;
    [[nodiscard]] NOUS_ENGINE_API uint32_t GetSampleRate() const;
    [[nodiscard]] NOUS_ENGINE_API uint8_t GetChannelCount() const;
    [[nodiscard]] NOUS_ENGINE_API void* GetInternalData() const;

private:
    // Set by ImporterAudio from .naud meta. Defaults to DECODED.
    AudioStreamingMode streamingMode;

    // Derived from the asset extension at import time. Stored so the
    // library filename can be reconstructed as Library/Audio/<UID>.<ext>
    // without re-sniffing the file.
    AudioFileType fileType;

    // Probed once by ImporterAudio after the library copy is in place.
    // Used by the Inspector / Assets Browser for length display, waveform
    // thumbnails, sample-rate warnings. Not read by the audio thread.
    float  durationSec;
    uint32_t sampleRate;
    uint8_t  channelCount;

    // Optional backend-side cache handle. Analogous to ResourceTexture::internalData.
    // For miniaudio: a pre-warmed ma_resource_manager_data_source* (or null).
    // MVP leaves this null and lets each ma_sound resolve by libraryPath; the
    // resource manager dedupes internally. Add only if profiling shows the
    // first-voice creation hitch.
    void* internalData;
};
