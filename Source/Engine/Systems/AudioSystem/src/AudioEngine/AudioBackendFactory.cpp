#include <AudioSystem/AudioEngine/AudioBackendFactory.h>

#include "AudioEngine/Backends/miniaudio/MiniaudioBackend.h"
#include <MemoryManager/MemoryManager.h>
#include <Logger/LogChannel.h>
#include <Logger/Logger.h>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_AUDIOSYSTEM;

IAudioEngineBackend* CreateAudioBackend(const AudioEngineBackend type)
{
    IAudioEngineBackend* backend = nullptr;

    switch (type)
    {
        case AudioEngineBackend::MINIAUDIO:
            backend = NOUS_NEW<MiniaudioBackend>(MemoryTag::AUDIO_SYSTEM);
            break;
        case AudioEngineBackend::UNKNOWN:
        default:
            NOUS_ERROR_C(CURRENT_CHANNEL, "Unknown audio backend type: %d", static_cast<int>(type));
            return nullptr;
    }

    if (!backend)
    {
        NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to create audio backend type (%d).", static_cast<int>(type));
        return nullptr;
    }

    NOUS_INFO_C(CURRENT_CHANNEL, "Created audio backend type (%d) successfully.", static_cast<int>(type));
    return backend;
}
