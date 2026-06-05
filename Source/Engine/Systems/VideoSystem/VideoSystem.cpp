#include "VideoSystem.h"

#include "VideoDecoder/Backends/ffmpeg/FFmpegBackend.h"
#include "VideoDecoder/IVideoDecoderBackend.h"
#include "Engine/Core/Logger/Asserts.h"
#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_VIDEOSYSTEM;

VideoSystem::VideoSystem() : m_backend(nullptr)
{
}

bool VideoSystem::Initialize(VideoDecoderBackend backend)
{
    NOUS_ASSERT(m_backend == nullptr);   // no double-init

    switch (backend)
    {
        case VideoDecoderBackend::FFMPEG:
            NOUS_INFO_C(CURRENT_CHANNEL, "Using video backend: FFMPEG");
            m_backend = NOUS_NEW<FFmpegBackend>(MemoryTag::VIDEO_SYSTEM);
            break;
        case VideoDecoderBackend::UNKNOWN:
        default:
            NOUS_ERROR_C(CURRENT_CHANNEL, "Invalid video backend requested.");
            return false;
    }

    if (!m_backend->Initialize())
    {
        NOUS_DELETE(m_backend, MemoryTag::VIDEO_SYSTEM);
        return false;
    }

    return true;
}

void VideoSystem::Shutdown()
{
    if (m_backend)
        m_backend->Shutdown();

    NOUS_DELETE(m_backend, MemoryTag::VIDEO_SYSTEM);
}

VideoHandle VideoSystem::CreateVideo(ResourceVideo* rVideo) const
{
    return m_backend ? m_backend->CreateVideo(rVideo) : nullptr;
}

void VideoSystem::DestroyVideo(VideoHandle handle) const noexcept
{
    if (m_backend)
        m_backend->DestroyVideo(handle);
}

void VideoSystem::Start(VideoHandle handle) const
{
    if (m_backend)
        m_backend->Start(handle);
}

void VideoSystem::Stop(VideoHandle handle) const
{
    if (m_backend)
        m_backend->Stop(handle);
}

void VideoSystem::SetLooping(VideoHandle handle, bool looping) const
{
    if (m_backend)
        m_backend->SetLooping(handle, looping);
}

bool VideoSystem::TryGetFrame(VideoHandle handle, double playheadSec, VideoFrame& out) const
{
    return m_backend && m_backend->TryGetFrame(handle, playheadSec, out);
}

void VideoSystem::GetDimensions(VideoHandle handle, uint32& width, uint32& height) const
{
    if (m_backend) m_backend->GetDimensions(handle, width, height);
    else { width = 0; height = 0; }
}

float VideoSystem::GetFrameRate(VideoHandle handle) const { return m_backend ? m_backend->GetFrameRate(handle) : 0.0f; }
float VideoSystem::GetDuration (VideoHandle handle) const { return m_backend ? m_backend->GetDuration(handle)  : 0.0f; }
bool  VideoSystem::IsFinished  (VideoHandle handle) const { return m_backend ? m_backend->IsFinished(handle)   : true; }
