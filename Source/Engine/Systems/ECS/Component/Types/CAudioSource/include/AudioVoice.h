#pragma once

#include <AudioSystem/SoundHandle.h>
#include "Engine/EngineExport.h"

class IAudioBroker;

/**
 * @brief Move-only RAII owner of exactly one backend voice.
 *
 * Releases the voice through the audio broker on destruction or reassignment, so
 * callers never hand-pair CreateSound with DestroySound — a forgotten release
 * leaks the NOUS_NEW'd ma_sound and trips the ShutdownMemory leak-abort.
 *
 * Holds the broker because release must go through it. For a CAudioSource member
 * the destructor runs during component teardown, which is safe: ModuleAudio (the
 * IAudioBroker impl) is constructed before ModuleScene and so outlives every scene
 * (Application module order) — the same invariant the old explicit OnDestroy
 * release relied on.
 */
class NOUS_ENGINE_API AudioVoice
{
public:
    AudioVoice() = default;
    AudioVoice(const IAudioBroker* audio, SoundHandle handle);
    ~AudioVoice();

    AudioVoice(AudioVoice&& other) noexcept;
    AudioVoice& operator=(AudioVoice&& other) noexcept;

    AudioVoice(const AudioVoice&)            = delete;
    AudioVoice& operator=(const AudioVoice&) = delete;

    // Releases the owned voice (if any) and clears the handle. Idempotent.
    void Reset() noexcept;

    [[nodiscard]] SoundHandle Get() const noexcept { return m_handle; }
    [[nodiscard]] explicit operator bool() const noexcept { return m_handle != nullptr; }

private:
    const IAudioBroker* m_audio  = nullptr;
    SoundHandle         m_handle = nullptr;
};
