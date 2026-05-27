#include "MiniaudioBackend.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Systems/AudioSystem/AudioSystem.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/Resource/ResourceAudio/include/ResourceAudio.h"

// miniaudio Library Documentation
// https://miniaud.io/docs/manual/index.html
//
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
