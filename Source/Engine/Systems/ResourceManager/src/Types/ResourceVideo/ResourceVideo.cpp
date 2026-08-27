#include <ResourceManager/Types/ResourceVideo/ResourceVideo.h>

#include <FileSystem/FileSystem.h>  // GetExtension

VideoFileType VideoFileTypeFromExtension(const std::string& libraryPath)
{
    const std::string ext = nous::engine::filesystem::GetExtension(libraryPath);
    if (ext == ".mp4" || ext == ".MP4") return VideoFileType::MP4;
    if (ext == ".gif" || ext == ".GIF") return VideoFileType::GIF;
    return VideoFileType::UNKNOWN;
}

VideoDecodeMode VideoDecodeModeFromFileType(const VideoFileType fileType)
{
    switch (fileType)
    {
        case VideoFileType::GIF: return VideoDecodeMode::PREDECODED;
        case VideoFileType::MP4: return VideoDecodeMode::STREAMED;
        case VideoFileType::UNKNOWN:
        default:                 return VideoDecodeMode::STREAMED;
    }
}

ResourceVideo::ResourceVideo(const uint32 uid) : ResourceBase(uid, ResourceType::VIDEO)
{
    fileType      = VideoFileType::UNKNOWN;
    decodeMode    = VideoDecodeMode::STREAMED;
    width         = 0;
    height        = 0;
    durationSec   = 0.0f;
    frameRate     = 0.0f;
    hasAudioTrack = false;
}

ResourceVideo::~ResourceVideo() = default;

void ResourceVideo::SetFileType(const VideoFileType _fileType)        { fileType = _fileType; }
void ResourceVideo::SetDecodeMode(const VideoDecodeMode _decodeMode)  { decodeMode = _decodeMode; }
void ResourceVideo::SetWidth(const uint32 _width)                     { width = _width; }
void ResourceVideo::SetHeight(const uint32 _height)                   { height = _height; }
void ResourceVideo::SetDurationSec(const float _durationSec)          { durationSec = _durationSec; }
void ResourceVideo::SetFrameRate(const float _frameRate)              { frameRate = _frameRate; }
void ResourceVideo::SetCodecName(const std::string_view _codecName)   { codecName = _codecName; }
void ResourceVideo::SetHasAudioTrack(const bool _hasAudioTrack)       { hasAudioTrack = _hasAudioTrack; }

VideoFileType   ResourceVideo::GetFileType() const      { return fileType; }
VideoDecodeMode ResourceVideo::GetDecodeMode() const    { return decodeMode; }
uint32          ResourceVideo::GetWidth() const         { return width; }
uint32          ResourceVideo::GetHeight() const        { return height; }
float           ResourceVideo::GetDurationSec() const   { return durationSec; }
float           ResourceVideo::GetFrameRate() const     { return frameRate; }
std::string     ResourceVideo::GetCodecName() const     { return codecName; }
bool            ResourceVideo::GetHasAudioTrack() const  { return hasAudioTrack; }
