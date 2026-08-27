#include <AudioSystem/AudioProbe/AudioProbe.h>

#include <Logger/LogChannel.h>
#include <Logger/Logger.h>

#include <miniaudio.h>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_AUDIOSYSTEM;

bool ProbeAudioFile(const std::string& libraryPath, AudioProbeInfo& outInfo)
{
    ma_decoder decoder;
    const ma_result initResult = ma_decoder_init_file(libraryPath.c_str(), nullptr, &decoder);
    if (initResult != MA_SUCCESS)
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "ProbeAudioFile() failed to open '%s' (code %d)",
            libraryPath.c_str(), static_cast<int>(initResult));
        return false;
    }

    ma_uint64 frameCount = 0;
    const ma_result lenResult = ma_decoder_get_length_in_pcm_frames(&decoder, &frameCount);

    outInfo.sampleRate   = static_cast<uint32_t>(decoder.outputSampleRate);
    outInfo.channelCount = static_cast<uint8_t>(decoder.outputChannels);
    outInfo.durationSec  = (lenResult == MA_SUCCESS && outInfo.sampleRate > 0)
        ? (static_cast<float>(frameCount) / static_cast<float>(outInfo.sampleRate))
        : 0.0f;

    ma_decoder_uninit(&decoder);
    return true;
}
