#pragma once

#include <AudioSystem/SoundHandle.h>
#include <AudioSystem/EffectChainHandle.h>
#include <AudioSystem/AudioTypes.h>
#include <AudioSystem/AudioGraph/AudioEffectTypes.h>

class ResourceAudio;

class IAudioEngineBackend
{
public:
    virtual ~IAudioEngineBackend() noexcept = default;

    virtual bool Initialize() = 0;
    virtual void PlayAudio(ResourceAudio* rAudio) = 0;
    virtual void Shutdown() noexcept = 0;

    // ----------------------------------------------------------------------
    // Per-voice sound lifecycle (used by CAudioSource).
    //
    // CreateSound builds a voice from the clip but does NOT start it.
    // The returned handle is owned by the caller and must be released with
    // DestroySound. All setters/queries no-op (or return false) on a null handle.
    // ----------------------------------------------------------------------
    virtual SoundHandle CreateSound(ResourceAudio* rAudio, AudioBus bus) = 0;
    virtual void        DestroySound(SoundHandle sound) noexcept = 0;

    virtual void        StartSound(SoundHandle sound) = 0;
    virtual void        StopSound(SoundHandle sound) = 0;

    virtual void        SetSoundVolume(SoundHandle sound, float volume) = 0;
    virtual void        SetSoundPitch(SoundHandle sound, float pitch) = 0;
    virtual void        SetSoundLooping(SoundHandle sound, bool looping) = 0;

    virtual bool        IsSoundPlaying(SoundHandle sound) const = 0;

    // Current playback position of the voice, in seconds (0 on a null handle).
    // The cursor is retained across StopSound (pause), so a paused voice reports
    // its held position — used by CVideoPlayer to slave its playhead to audio.
    virtual double      GetCursorSeconds(SoundHandle sound) const = 0;

    // ----------------------------------------------------------------------
    // 3D spatialization. The listener is engine-global (index 0); only the
    // active main listener writes it each frame. Per-voice setters configure
    // a voice's position and distance attenuation. All no-op on a null handle.
    // ----------------------------------------------------------------------
    virtual void SetListenerPosition (float x, float y, float z) = 0;
    virtual void SetListenerDirection(float x, float y, float z) = 0;   // forward
    virtual void SetListenerWorldUp  (float x, float y, float z) = 0;

    virtual void SetSoundSpatializationEnabled(SoundHandle sound, bool enabled) = 0;
    virtual void SetSoundPosition        (SoundHandle sound, float x, float y, float z) = 0;
    virtual void SetSoundMinDistance     (SoundHandle sound, float distance) = 0;
    virtual void SetSoundMaxDistance     (SoundHandle sound, float distance) = 0;
    virtual void SetSoundAttenuationModel(SoundHandle sound, AttenuationModel model) = 0;

    // ----------------------------------------------------------------------
    // Bus mixer (Step 6). Four named buses (Music/SFX/UI/Ambient) parented to
    // the engine master; Master is the engine endpoint. Volume is linear gain.
    // ----------------------------------------------------------------------
    virtual void  SetBusVolume(AudioBus bus, float volume) = 0;
    virtual void  SetBusMute  (AudioBus bus, bool mute) = 0;
    virtual void  SetBusSolo  (AudioBus bus, bool solo) = 0;
    virtual float GetBusVolume(AudioBus bus) const = 0;
    virtual bool  GetBusMute  (AudioBus bus) const = 0;
    virtual bool  GetBusSolo  (AudioBus bus) const = 0;

    // ----------------------------------------------------------------------
    // Effect chain (Layer 2). A per-voice DSP chain spliced between a voice and
    // its bus: sound → effect[0] → ... → effect[N-1] → bus. CreateEffectChain
    // builds + wires the nodes and returns an owning handle (null on failure or
    // empty desc). DestroyEffectChain reattaches the voice straight to its bus,
    // then frees the nodes — so the SoundHandle MUST still be alive when called.
    // ----------------------------------------------------------------------
    virtual EffectChainHandle CreateEffectChain(SoundHandle sound, const AudioGraphDesc& desc, AudioBus outputBus) = 0;
    virtual void              SetEffectParam(EffectChainHandle chain, int effectIndex, int paramIndex, float value) = 0;
    virtual void              DestroyEffectChain(EffectChainHandle chain) noexcept = 0;
};
