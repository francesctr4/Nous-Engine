#include <ResourceManager/Types/ResourceAudio/ResourceAudio.h>

ResourceAudio::ResourceAudio(const uint32 uid) : ResourceBase(uid, ResourceType::AUDIO)
{
    streamingMode = AudioStreamingMode::DECODED;
    fileType = AudioFileType::UNKNOWN;

    durationSec  = 0.0f;
    sampleRate   = 0;
    channelCount = 0;

    internalData = nullptr;
}

ResourceAudio::~ResourceAudio() = default;

void ResourceAudio::SetStreamingMode(const AudioStreamingMode _streamingMode)
{
    streamingMode = _streamingMode;
}

void ResourceAudio::SetFileType(const AudioFileType _fileType)
{
    fileType = _fileType;
}

void ResourceAudio::SetDurationSec(const float _durationSec)
{
    durationSec = _durationSec;
}

void ResourceAudio::SetSampleRate(const uint32 _sampleRate)
{
    sampleRate = _sampleRate;
}

void ResourceAudio::SetChannelCount(const uint8 _channelCount)
{
    channelCount = _channelCount;
}

void ResourceAudio::SetInternalData(void* _internalData)
{
    internalData = _internalData;
}

AudioStreamingMode ResourceAudio::GetStreamingMode() const
{
    return streamingMode;
}

AudioFileType ResourceAudio::GetFileType() const
{
    return fileType;
}

float ResourceAudio::GetDurationSec() const
{
    return durationSec;
}

uint32 ResourceAudio::GetSampleRate() const
{
    return sampleRate;
}

uint8 ResourceAudio::GetChannelCount() const
{
    return channelCount;
}

void* ResourceAudio::GetInternalData() const
{
    return internalData;
}
