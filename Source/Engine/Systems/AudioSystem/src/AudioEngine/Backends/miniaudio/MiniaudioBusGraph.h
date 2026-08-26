#pragma once

#include <AudioSystem/AudioTypes.h>
#include "AudioMixer/AudioBusMath.h"

#include <miniaudio.h>

// Miniaudio realization of the bus mixer: owns the four ma_sound_groups
// (Music/SFX/UI/Ambient) parented to the engine endpoint. Master is the engine
// itself (ma_engine_set_volume) — no group.
//
// This is a private helper of MiniaudioBackend, hence it lives under
// Backends/miniaudio/ and is free to include <miniaudio.h>. The backend-agnostic
// mute/solo policy lives separately in AudioMixer/AudioBusMath.h (ComputeEffectiveGain),
// which any future backend's bus graph reuses verbatim. A second backend writes its
// own XxxBusGraph; nothing in the shared layer is reworked.
class MiniaudioBusGraph
{
public:
    // Creates the four groups parented to the engine endpoint and pushes initial
    // unity gains. Returns false (and leaves the graph inert) on failure.
    bool Initialize(ma_engine* engine);

    // Uninits the four groups. Must run BEFORE ma_engine_uninit. Safe to call twice.
    void Shutdown() noexcept;

    // Group a voice attaches to at init. nullptr for Master (→ engine endpoint)
    // or before Initialize succeeds.
    ma_sound_group* GetGroup(AudioBus bus);

    void  SetBusVolume(AudioBus bus, float volume);
    void  SetBusMute  (AudioBus bus, bool mute);
    void  SetBusSolo  (AudioBus bus, bool solo);    // no-op for Master

    float GetBusVolume(AudioBus bus) const;
    bool  GetBusMute  (AudioBus bus) const;
    bool  GetBusSolo  (AudioBus bus) const;

private:
    static constexpr int k_busCount   = 5;  // Master + 4 named (== AudioBus count)
    static constexpr int k_groupCount = 4;  // named buses only

    void RecomputeEffectiveGains();

    // m_state index  = static_cast<int>(bus)       (Master=0 .. Ambient=4)
    // m_groups index = static_cast<int>(bus) - 1   (Music=0 .. Ambient=3; N/A for Master)
    ma_engine*     m_engine = nullptr;          // non-owning
    ma_sound_group m_groups[k_groupCount]{};
    BusState       m_state[k_busCount]{};
    bool           m_initialized = false;
};
