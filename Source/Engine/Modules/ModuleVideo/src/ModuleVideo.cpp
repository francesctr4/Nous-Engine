#include "Engine/Modules/ModuleVideo/include/ModuleVideo.h"

#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/Core/EventSystem/Event/include/Event.h"
#include "Engine/Core/MemoryManager/MemoryManager.h"

#include "Engine/Systems/VideoSystem/VideoSystem.h"

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MODULE_VIDEO;

ModuleVideo::ModuleVideo(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem)
    : Module(eventSystem, jobSystem), m_videoSystem(nullptr)
{
}

ModuleVideo::~ModuleVideo() = default;

bool ModuleVideo::Awake()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Initializing Video System ...");

    m_videoSystem = NOUS_NEW<VideoSystem>(MemoryTag::VIDEO_SYSTEM);

    if (!m_videoSystem->Initialize(VideoDecoderBackend::FFMPEG))
        NOUS_WARN_C(CURRENT_CHANNEL, "Video system initialization failed — running without video.");

    return true;
}

bool ModuleVideo::Start() { return true; }
UpdateStatus ModuleVideo::PreUpdate(float dt)  { return UpdateStatus::CONTINUE; }
UpdateStatus ModuleVideo::Update(float dt)     { return UpdateStatus::CONTINUE; }
UpdateStatus ModuleVideo::PostUpdate(float dt) { return UpdateStatus::CONTINUE; }

bool ModuleVideo::CleanUp()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Shutdown Video System ...");

    m_videoSystem->Shutdown();
    NOUS_DELETE<VideoSystem>(m_videoSystem, MemoryTag::VIDEO_SYSTEM);

    return true;
}

VideoHandle ModuleVideo::CreateVideo(ResourceVideo* rVideo) const
{
    return m_videoSystem ? m_videoSystem->CreateVideo(rVideo) : nullptr;
}

void ModuleVideo::DestroyVideo(VideoHandle handle) const noexcept
{
    if (m_videoSystem) m_videoSystem->DestroyVideo(handle);
}

void ModuleVideo::Start(VideoHandle handle) const            { if (m_videoSystem) m_videoSystem->Start(handle); }
void ModuleVideo::Stop(VideoHandle handle) const             { if (m_videoSystem) m_videoSystem->Stop(handle); }
void ModuleVideo::SetLooping(VideoHandle handle, bool l) const { if (m_videoSystem) m_videoSystem->SetLooping(handle, l); }

bool ModuleVideo::TryGetFrame(VideoHandle handle, double playheadSec, VideoFrame& out) const
{
    return m_videoSystem && m_videoSystem->TryGetFrame(handle, playheadSec, out);
}

void ModuleVideo::GetDimensions(VideoHandle handle, uint32& width, uint32& height) const
{
    if (m_videoSystem) m_videoSystem->GetDimensions(handle, width, height);
    else { width = 0; height = 0; }
}

float ModuleVideo::GetFrameRate(VideoHandle handle) const { return m_videoSystem ? m_videoSystem->GetFrameRate(handle) : 0.0f; }
float ModuleVideo::GetDuration (VideoHandle handle) const { return m_videoSystem ? m_videoSystem->GetDuration(handle)  : 0.0f; }
bool  ModuleVideo::IsFinished  (VideoHandle handle) const { return m_videoSystem ? m_videoSystem->IsFinished(handle)   : true; }

void ModuleVideo::OnEvent(const Event& event)
{
    switch (event.type)
    {
        default:
            break;
    }
}
