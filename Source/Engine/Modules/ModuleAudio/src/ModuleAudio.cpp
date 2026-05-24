#include "Engine/Modules/ModuleAudio/include/ModuleAudio.h"

#include "Engine/Core/Logger/Logger.h"
#include "Engine/Core/EventSystem/EventSystem.h"
#include "Engine/Core/EventSystem/Event/include/Event.h"

// miniaudio Library Documentation
// https://miniaud.io/docs/manual/index.html
//
// Vorbis (.ogg) support: miniaudio has no built-in Vorbis decoder. It auto-detects
// stb_vorbis when stb_vorbis.c is included in the same TU as MA_IMPLEMENTATION,
// using this header-only / implementation sandwich.
#define STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

#define MA_IMPLEMENTATION
#include <miniaudio.h>

#undef STB_VORBIS_HEADER_ONLY
#include <stb_vorbis.c>

ma_engine m_audioEngine;

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_MODULE_AUDIO;

ModuleAudio::ModuleAudio(EventSystem* eventSystem, nous::engine::multithreading::NOUS_JobSystem* jobSystem)
    : Module(eventSystem, jobSystem) {}

ModuleAudio::~ModuleAudio() = default;

bool ModuleAudio::Awake()
{
    const ma_result result = ma_engine_init(nullptr, &m_audioEngine);

    if (result != MA_SUCCESS)
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to initialize audio engine.");

    NOUS_INFO_C(CURRENT_CHANNEL, "Audio engine initialized successfully.");

    return true;
}

bool ModuleAudio::Start()
{
    ma_engine_play_sound(&m_audioEngine, "Assets/Audio/SFX/test.wav", nullptr);

    const ma_result result =
    ma_engine_play_sound(&m_audioEngine, "Assets/Audio/Music/music.ogg", nullptr);

    if (result != MA_SUCCESS)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to play music. Error code: {}", result);
    }

    return true;
}

UpdateStatus ModuleAudio::PreUpdate(float dt)
{
    return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleAudio::Update(float dt)
{
    return UpdateStatus::CONTINUE;
}

UpdateStatus ModuleAudio::PostUpdate(float dt)
{
    return UpdateStatus::CONTINUE;
}

bool ModuleAudio::CleanUp()
{
    ma_engine_uninit(&m_audioEngine);

    NOUS_INFO_C(CURRENT_CHANNEL, "Audio engine shutdown successfully.");
	return true;
}

void ModuleAudio::OnEvent(const Event &event)
{
    switch (event.type)
    {
        default:
            {
                break;
            }
    }
}

