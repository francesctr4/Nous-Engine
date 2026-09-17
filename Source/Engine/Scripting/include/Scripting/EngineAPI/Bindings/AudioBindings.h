#pragma once

#include <cstdint>

// Script-facing audio control, per GameObject's CAudioSource.
//
// Deliberately five calls. A source is authored in the editor -- its clip, bus,
// spatialization and effect graph -- and a script only triggers and shapes it. There
// is no PlayOneShot(clipPath): a voice that outlives the frame needs a pooling owner,
// and a CAudioSource child per sound covers every case this engine has.
struct AudioAPI
{
    // RESTARTS from the beginning if already playing, which is what a footstep wants.
    // Requires the source's playOnAwake to be false, or the scene's own state machine
    // will also start it.
    void (*Play)     (uint32_t goId) = nullptr;
    void (*Stop)     (uint32_t goId) = nullptr;
    bool (*IsPlaying)(uint32_t goId) = nullptr;

    // Both are pushed to the live voice every frame by CAudioSource::OnUpdate, so a
    // value set here persists until something else changes it.
    void (*SetVolume)(uint32_t goId, float volume) = nullptr;
    // Cheap variation, so ten identical footsteps do not sound mechanical.
    void (*SetPitch) (uint32_t goId, float pitch)  = nullptr;
};

class IScriptSceneHost;
void SetupAudioBindings(AudioAPI& audio, IScriptSceneHost* sceneHost);
