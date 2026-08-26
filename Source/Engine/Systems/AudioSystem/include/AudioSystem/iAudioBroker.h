#pragma once

#include <AudioSystem/SoundHandle.h>
#include <AudioSystem/EffectChainHandle.h>
#include <AudioSystem/AudioTypes.h>
#include <AudioSystem/AudioGraph/AudioEffectTypes.h>

class ResourceAudio;

// -----------------------------------------------------------------------------
// The audio engine, seen from inside Systems/.
// -----------------------------------------------------------------------------
/**
 * @brief Voice / listener / effect-chain surface, as needed by components.
 *
 * Implemented by ModuleAudio so CAudioSource and CAudioListener can drive voices
 * without depending on Modules/. Mirrors the existing IResourceLoader seam.
 *
 * Deliberately EXCLUDES the bus-mixer surface (SetBusVolume / GetBusMute / ...)
 * and PlayAudio: only the editor's AudioMixerWindow uses those, and it talks to
 * ModuleAudio directly. The interface is sized to this client set, not to
 * everything ModuleAudio happens to offer.
 */
class IAudioBroker
{
public:
    virtual ~IAudioBroker() = default;

    // ─────────────────────────────── Voice lifecycle ─────────────────────────
    virtual SoundHandle CreateSound(ResourceAudio* rAudio, AudioBus bus) const = 0;
    virtual void        DestroySound(SoundHandle sound) const noexcept = 0;

    virtual void        StartSound(SoundHandle sound) const = 0;
    virtual void        StopSound(SoundHandle sound) const = 0;

    // ─────────────────────────────── Voice parameters ────────────────────────
    virtual void        SetSoundVolume(SoundHandle sound, float volume) const = 0;
    virtual void        SetSoundPitch(SoundHandle sound, float pitch) const = 0;
    virtual void        SetSoundLooping(SoundHandle sound, bool looping) const = 0;

    virtual bool        IsSoundPlaying(SoundHandle sound) const = 0;

    /** @brief Playback position of the voice in seconds (0 when no backend / null handle). */
    virtual double      GetCursorSeconds(SoundHandle sound) const = 0;

    // ─────────────────────────────── Listener (3D) ───────────────────────────
    virtual void SetListenerPosition (float x, float y, float z) = 0;
    virtual void SetListenerDirection(float x, float y, float z) const = 0;
    virtual void SetListenerWorldUp  (float x, float y, float z) const = 0;

    // ─────────────────────────────── Voice spatialization ────────────────────
    virtual void SetSoundSpatializationEnabled(SoundHandle sound, bool enabled) const = 0;
    virtual void SetSoundPosition        (SoundHandle sound, float x, float y, float z) const = 0;
    virtual void SetSoundMinDistance     (SoundHandle sound, float distance) const = 0;
    virtual void SetSoundMaxDistance     (SoundHandle sound, float distance) const = 0;
    virtual void SetSoundAttenuationModel(SoundHandle sound, AttenuationModel model) const = 0;

    // ─────────────────────────────── Effect chains ───────────────────────────
    virtual EffectChainHandle CreateEffectChain(SoundHandle sound,
                                                const AudioGraphDesc& desc,
                                                AudioBus outputBus) const = 0;
    virtual void              SetEffectParam(EffectChainHandle chain, int effectIndex,
                                             int paramIndex, float value) const = 0;
    virtual void              DestroyEffectChain(EffectChainHandle chain) const noexcept = 0;
};
