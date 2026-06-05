#include "Engine/Systems/ResourceManager/Types/ResourceVideo/include/ImporterVideo.h"

#include "Engine/Core/FileSystem/FileSystem.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Systems/ResourceManager/Core/ResourceBase/include/MetaFileData.h"
#include "Engine/Systems/ResourceManager/Types/ResourceVideo/include/ResourceVideo.h"
#include "Engine/Systems/VideoSystem/VideoProbe.h"

bool ImporterVideo::Import(const MetaFileData& metaFileData)
{
    ResourceBase* tempVideo = NOUS_NEW<ResourceVideo>(MemoryTag::RESOURCE_VIDEO);
    return Save(metaFileData, tempVideo);
}

bool ImporterVideo::Save(const MetaFileData& metaFileData, ResourceBase*& inResource)
{
    NOUS_DELETE(inResource, MemoryTag::RESOURCE_VIDEO);

    const bool ret = nous::engine::filesystem::CopyFile(metaFileData.assetsPath, metaFileData.libraryPath);
    if (!ret)
    {
        NOUS_WARN("ImporterVideo::Save() failed to copy '%s' -> '%s'",
            metaFileData.assetsPath.c_str(), metaFileData.libraryPath.c_str());
    }
    return ret;
}

bool ImporterVideo::Deserialize(const std::string& libraryPath, ResourceBase* outResource)
{
    ResourceVideo* video = down_cast<ResourceVideo*>(outResource);

    const VideoFileType fileType = VideoFileTypeFromExtension(libraryPath);
    video->SetFileType(fileType);
    video->SetDecodeMode(VideoDecodeModeFromFileType(fileType));

    VideoProbeInfo info;
    if (!ProbeVideoFile(libraryPath, info))
    {
        NOUS_WARN("ImporterVideo::Deserialize() probe failed for '%s'", libraryPath.c_str());
        return false;
    }

    video->SetWidth(info.width);
    video->SetHeight(info.height);
    video->SetDurationSec(info.durationSec);
    video->SetFrameRate(info.frameRate);
    video->SetCodecName(info.codecName);
    video->SetHasAudioTrack(info.hasAudioTrack);
    return true;
}

void ImporterVideo::Evict(ResourceBase* /*inResource*/)
{
    // Pure descriptor — no CPU buffer held. Nothing to release.
}

bool ImporterVideo::Upload(ResourceBase* /*outResource*/, IGPUResourceFactory* /*gpu*/)
{
    // No per-resource GPU residency: the per-instance frame texture is created and
    // owned by the renderer in Phase 3 (per CVideoPlayer), not by ResourceVideo.
    return true;
}

void ImporterVideo::Release(ResourceBase* /*inResource*/, IGPUResourceFactory* /*gpu*/)
{
    // No GPU/backend handles held; nothing to release.
}
