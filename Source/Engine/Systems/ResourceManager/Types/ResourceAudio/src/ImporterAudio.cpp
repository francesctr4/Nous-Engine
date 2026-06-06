#include "Engine/Systems/ResourceManager/Types/ResourceAudio/include/ImporterAudio.h"

#include "Engine/Core/FileSystem/FileSystem.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Systems/AudioSystem/AudioProbe/include/AudioProbe.h"
#include "Engine/Systems/ResourceManager/Core/ResourceBase/include/MetaFileData.h"
#include "Engine/Systems/ResourceManager/Types/ResourceAudio/include/ResourceAudio.h"

namespace
{
    AudioFileType FileTypeFromExtension(const std::string& libraryPath)
    {
        const std::string ext = nous::engine::filesystem::GetExtension(libraryPath);
        if (ext == ".wav" || ext == ".WAV") return AudioFileType::WAV;
        if (ext == ".ogg" || ext == ".OGG") return AudioFileType::OGG;
        return AudioFileType::UNKNOWN;
    }

    // Streaming policy, decided purely by file type at import time:
    //   WAV → DECODED  (typically short SFX: decode fully into memory)
    //   OGG → STREAMED (typically music / long ambient: stream from disk)
    // UNKNOWN falls back to DECODED — the safe default for an unrecognized clip.
    AudioStreamingMode StreamingModeFromFileType(AudioFileType fileType)
    {
        switch (fileType)
        {
            case AudioFileType::OGG: return AudioStreamingMode::STREAMED;
            case AudioFileType::WAV: return AudioStreamingMode::DECODED;
            case AudioFileType::UNKNOWN:
            default:                 return AudioStreamingMode::DECODED;
        }
    }
}

bool ImporterAudio::Import(const MetaFileData& metaFileData)
{
    ResourceBase* tempAudio = NOUS_NEW<ResourceAudio>(MemoryTag::RESOURCE_AUDIO);
    return Save(metaFileData, tempAudio);
}

bool ImporterAudio::Save(const MetaFileData& metaFileData, ResourceBase*& inResource)
{
    NOUS_DELETE(inResource, MemoryTag::RESOURCE_AUDIO);

    const bool ret = nous::engine::filesystem::CopyFile(metaFileData.assetsPath, metaFileData.libraryPath);
    if (!ret)
    {
        NOUS_WARN("ImporterAudio::Save() failed to copy '%s' -> '%s'",
            metaFileData.assetsPath.c_str(), metaFileData.libraryPath.c_str());
    }
    return ret;
}

bool ImporterAudio::Deserialize(const std::string& libraryPath, ResourceBase* outResource)
{
    ResourceAudio* audio = down_cast<ResourceAudio*>(outResource);

    const AudioFileType fileType = FileTypeFromExtension(libraryPath);
    audio->SetFileType(fileType);
    audio->SetStreamingMode(StreamingModeFromFileType(fileType));

    AudioProbeInfo info;
    if (!ProbeAudioFile(libraryPath, info))
    {
        NOUS_WARN("ImporterAudio::Deserialize() probe failed for '%s'", libraryPath.c_str());
        return false;
    }

    audio->SetDurationSec(info.durationSec);
    audio->SetSampleRate(info.sampleRate);
    audio->SetChannelCount(info.channelCount);
    return true;
}

void ImporterAudio::Evict(ResourceBase* /*inResource*/)
{
    // MVP holds no CPU-side PCM buffer; nothing to release.
}

bool ImporterAudio::Upload(ResourceBase* /*outResource*/, IGPUResourceFactory* /*gpu*/)
{
    // miniaudio resolves by libraryPath at ma_sound creation; no GPU/backend pre-warm in MVP.
    return true;
}

void ImporterAudio::Release(ResourceBase* /*inResource*/, IGPUResourceFactory* /*gpu*/)
{
    // No GPU/backend handles held; nothing to release.
}
