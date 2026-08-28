#include "Types/ResourceVideo/ImporterVideo.h"
#include <EngineCore/Casts.h>

#include <FileSystem/FileSystem.h>
#include <Logger/Logger.h>
#include <MemoryManager/MemoryManager.h>

#include <ResourceManager/Core/MetaFileData.h>
#include <ResourceManager/Types/ResourceVideo/ResourceVideo.h>
#include <VideoSystem/AudioExtract/AudioExtract.h>
#include <VideoSystem/VideoProbe/VideoProbe.h>

#include <filesystem>

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
        return ret;
    }

    // Extract the audio track to a companion .ogg next to the source video in
    // Assets/, regenerating only when missing or older than the video (mirrors
    // the ImportPipeline Case-3 timestamp rule). No audio track / failure is
    // non-fatal — the video simply has no companion and plays silently.
    const std::string oggAssetsPath = MakeCompanionOggPath(metaFileData.assetsPath);

    namespace fs = std::filesystem;
    std::error_code ec;
    const bool      oggExists  = fs::exists(oggAssetsPath, ec);
    const long long oggMtime   = oggExists ? fs::last_write_time(oggAssetsPath, ec).time_since_epoch().count() : 0;
    const long long videoMtime = fs::last_write_time(metaFileData.assetsPath, ec).time_since_epoch().count();

    if (ShouldRegenerateCompanion(oggExists, oggMtime, videoMtime))
    {
        if (ExtractVideoAudioTrack(metaFileData.libraryPath, oggAssetsPath))
            NOUS_INFO("ImporterVideo::Save() extracted audio -> '%s'", oggAssetsPath.c_str());
        else
            NOUS_INFO("ImporterVideo::Save() no audio extracted for '%s'", metaFileData.assetsPath.c_str());
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
