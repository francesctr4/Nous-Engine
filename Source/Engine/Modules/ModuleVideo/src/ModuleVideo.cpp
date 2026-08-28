#include <ModuleVideo/ModuleVideo.h>

#include <Logger/Logger.h>
#include <EventSystem/EventSystem.h>
#include <EventSystem/Event.h>
#include <MemoryManager/MemoryManager.h>

#include <VideoSystem/VideoDecoder/IVideoDecoderBackend.h>
#include <VideoSystem/VideoDecoder/VideoDecoderBackendFactory.h>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MODULE_VIDEO;

ModuleVideo::ModuleVideo(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem)
    : Module(eventSystem, jobSystem), m_backend(nullptr)
{
}

ModuleVideo::~ModuleVideo() = default;

bool ModuleVideo::Awake()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Initializing Video System ...");

    m_backend = CreateVideoDecoderBackend(VideoDecoderBackend::FFMPEG);

    if (!m_backend || !m_backend->Initialize())
    {
        NOUS_WARN_C(CURRENT_CHANNEL, "Video system initialization failed — running without video.");
        if (m_backend)
        {
            NOUS_DELETE(m_backend, MemoryTag::VIDEO_SYSTEM);
            m_backend = nullptr;
        }
    }

    return true;
}

bool ModuleVideo::Start() { return true; }
UpdateStatus ModuleVideo::PreUpdate(float dt)  { return UpdateStatus::CONTINUE; }
UpdateStatus ModuleVideo::Update(float dt)     { return UpdateStatus::CONTINUE; }
UpdateStatus ModuleVideo::PostUpdate(float dt) { return UpdateStatus::CONTINUE; }

bool ModuleVideo::CleanUp()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Shutdown Video System ...");

    if (m_backend)
    {
        m_backend->Shutdown();
        NOUS_DELETE(m_backend, MemoryTag::VIDEO_SYSTEM);
        m_backend = nullptr;
    }

    return true;
}

VideoHandle ModuleVideo::CreateVideo(ResourceVideo* rVideo) const
{
    return m_backend ? m_backend->CreateVideo(rVideo) : nullptr;
}

void ModuleVideo::DestroyVideo(VideoHandle handle) const noexcept
{
    if (m_backend) m_backend->DestroyVideo(handle);
}

void ModuleVideo::Start(VideoHandle handle) const             { if (m_backend) m_backend->Start(handle); }
void ModuleVideo::Stop(VideoHandle handle) const              { if (m_backend) m_backend->Stop(handle); }
void ModuleVideo::SetLooping(VideoHandle handle, bool l) const { if (m_backend) m_backend->SetLooping(handle, l); }

bool ModuleVideo::TryGetFrame(VideoHandle handle, double playheadSec, VideoFrame& out) const
{
    return m_backend && m_backend->TryGetFrame(handle, playheadSec, out);
}

void ModuleVideo::GetDimensions(VideoHandle handle, uint32_t& width, uint32_t& height) const
{
    if (m_backend) m_backend->GetDimensions(handle, width, height);
    else { width = 0; height = 0; }
}

float ModuleVideo::GetFrameRate(VideoHandle handle) const { return m_backend ? m_backend->GetFrameRate(handle) : 0.0f; }
float ModuleVideo::GetDuration (VideoHandle handle) const { return m_backend ? m_backend->GetDuration(handle)  : 0.0f; }
bool  ModuleVideo::IsFinished  (VideoHandle handle) const { return m_backend ? m_backend->IsFinished(handle)   : true; }

void ModuleVideo::OnEvent(const Event& event)
{
    switch (event.type)
    {
        default:
            break;
    }
}
