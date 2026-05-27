#include "Engine/Systems/ResourceManager/ResourceTypes/Importer/ImporterAudio/include/ImporterAudio.h"

#include "Engine/Core/FileSystem/FileSystem.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Systems/AudioSystem/AudioSystem.h"
#include "Engine/Systems/ResourceManager/ResourceTypes/Resource/MetaFileData.inl"
#include "Engine/Systems/ResourceManager/ResourceTypes/Resource/ResourceAudio/include/ResourceAudio.h"

namespace
{
    AudioFileType FileTypeFromExtension(const std::string& libraryPath)
    {
        const std::string ext = nous::engine::filesystem::GetExtension(libraryPath);
        if (ext == ".wav" || ext == ".WAV") return AudioFileType::WAV;
        if (ext == ".ogg" || ext == ".OGG") return AudioFileType::OGG;
        return AudioFileType::UNKNOWN;
    }
}

bool ImporterAudio::Import(const MetaFileData& metaFileData)
{
    Resource* tempAudio = NOUS_NEW<ResourceAudio>(MemoryTag::RESOURCE_AUDIO);
    return Save(metaFileData, tempAudio);
}

bool ImporterAudio::Save(const MetaFileData& metaFileData, Resource*& inResource)
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

bool ImporterAudio::Deserialize(const std::string& libraryPath, Resource* outResource)
{
    ResourceAudio* audio = down_cast<ResourceAudio*>(outResource);

    audio->SetFileType(FileTypeFromExtension(libraryPath));

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

void ImporterAudio::Evict(Resource* /*inResource*/)
{
    // MVP holds no CPU-side PCM buffer; nothing to release.
}

bool ImporterAudio::Upload(Resource* /*outResource*/, IGPUResourceFactory* /*gpu*/)
{
    // miniaudio resolves by libraryPath at ma_sound creation; no GPU/backend pre-warm in MVP.
    return true;
}

void ImporterAudio::Release(Resource* /*inResource*/, IGPUResourceFactory* /*gpu*/)
{
    // No GPU/backend handles held; nothing to release.
}
