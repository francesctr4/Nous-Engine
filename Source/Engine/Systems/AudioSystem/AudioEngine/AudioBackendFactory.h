#pragma once

#include "Engine/Systems/AudioSystem/AudioEngine/IAudioEngineBackend.h"

#include <cstdint>

// Which audio backend to instantiate. Only miniaudio exists today; a future
// backend (XAudio2 / FMOD / Wwise) gets an enumerator here.
enum class AudioEngineBackend : std::int8_t
{
    UNKNOWN   = -1,
    MINIAUDIO = 0
};

/**
 * @brief Instantiates the concrete audio backend for the given type.
 *
 * Replaces the old AudioSystem passthrough facade: ModuleAudio now owns an
 * IAudioEngineBackend* directly and calls this factory to create it. Mirrors
 * CreateRendererBackend / CreateVideoDecoderBackend, and stays the one TU that
 * names the concrete backend. The returned backend is NOT yet Initialize()d —
 * the caller (ModuleAudio::Awake) does that and owns the lifetime (NOUS_DELETE).
 *
 * @return Heap-allocated backend (NOUS_NEW, MemoryTag::AUDIO_SYSTEM) or nullptr
 *         on an unknown/unsupported type.
 */
[[nodiscard]] IAudioEngineBackend* CreateAudioBackend(AudioEngineBackend type);
