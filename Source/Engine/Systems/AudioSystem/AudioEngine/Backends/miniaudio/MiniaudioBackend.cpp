#include "MiniaudioBackend.h"
#include "Engine/Core/Logger/Logger.h"

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

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_AUDIOSYSTEM;

MiniaudioBackend::MiniaudioBackend() : m_audioEngine({})
{

}

MiniaudioBackend::~MiniaudioBackend()
{

}

bool MiniaudioBackend::Initialize()
{
    const ma_result result = ma_engine_init(nullptr, &m_audioEngine);

    if (result != MA_SUCCESS)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to initialize audio engine. Error code: {}", result);
        return false;
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "miniaudio engine initialized successfully.");
    return true;
}

void MiniaudioBackend::PlayAudio(ResourceAudio* rAudio)
{
    const ma_result result =
        ma_engine_play_sound(&m_audioEngine, rAudio->assetsPath.c_str(), nullptr);

    if (result != MA_SUCCESS)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to play music. Error code: %s", result);
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "Successfully playing audio: '%s' from '%s'",
        rAudio->name.c_str(), rAudio->assetsPath.c_str());
}

void MiniaudioBackend::Shutdown() noexcept
{
    ma_engine_uninit(&m_audioEngine);

    NOUS_INFO_C(CURRENT_CHANNEL, "Audio engine shutdown successfully.");
}
