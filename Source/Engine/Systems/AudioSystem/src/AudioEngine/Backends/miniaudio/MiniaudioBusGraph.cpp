#include "AudioEngine/Backends/miniaudio/MiniaudioBusGraph.h"

#include <Logger/Logger.h>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_AUDIOSYSTEM;

bool MiniaudioBusGraph::Initialize(ma_engine* engine)
{
    if (!engine)
        return false;

    m_engine = engine;

    for (int i = 0; i < k_groupCount; ++i)
    {
        // Parent = nullptr → attach to the engine endpoint (master).
        const ma_result r = ma_sound_group_init(m_engine, 0, nullptr, &m_groups[i]);
        if (r != MA_SUCCESS)
        {
            NOUS_ERROR_C(CURRENT_CHANNEL, "Failed to init audio bus group %d. Error code: %d",
                i, static_cast<int>(r));
            for (int j = 0; j < i; ++j)
                ma_sound_group_uninit(&m_groups[j]);
            m_engine = nullptr;
            return false;
        }

        // Buses are pure volume/mix nodes — disable spatialization so a bus is not
        // attenuated by listener distance (groups default to spatialization ON, with
        // position at the origin). Per-voice spatialization is set on the ma_sound.
        ma_sound_group_set_spatialization_enabled(&m_groups[i], MA_FALSE);
    }

    m_initialized = true;
    RecomputeEffectiveGains();  // push initial unity gains (state defaults to 1.0/unmuted)
    return true;
}

void MiniaudioBusGraph::Shutdown() noexcept
{
    if (!m_initialized)
        return;

    for (int i = 0; i < k_groupCount; ++i)
        ma_sound_group_uninit(&m_groups[i]);

    m_initialized = false;
    m_engine = nullptr;
}

ma_sound_group* MiniaudioBusGraph::GetGroup(AudioBus bus)
{
    if (!m_initialized || bus == AudioBus::Master)
        return nullptr;
    return &m_groups[static_cast<int>(bus) - 1];
}

void MiniaudioBusGraph::SetBusVolume(AudioBus bus, float volume)
{
    m_state[static_cast<int>(bus)].userVolume = volume;
    RecomputeEffectiveGains();
}

void MiniaudioBusGraph::SetBusMute(AudioBus bus, bool mute)
{
    m_state[static_cast<int>(bus)].mute = mute;
    RecomputeEffectiveGains();
}

void MiniaudioBusGraph::SetBusSolo(AudioBus bus, bool solo)
{
    if (bus == AudioBus::Master)
        return;  // solo at the tree root is meaningless
    m_state[static_cast<int>(bus)].solo = solo;
    RecomputeEffectiveGains();
}

float MiniaudioBusGraph::GetBusVolume(AudioBus bus) const { return m_state[static_cast<int>(bus)].userVolume; }
bool  MiniaudioBusGraph::GetBusMute  (AudioBus bus) const { return m_state[static_cast<int>(bus)].mute; }
bool  MiniaudioBusGraph::GetBusSolo  (AudioBus bus) const { return m_state[static_cast<int>(bus)].solo; }

void MiniaudioBusGraph::RecomputeEffectiveGains()
{
    if (!m_initialized)
        return;

    // anySolo over the four NAMED buses only (Master never solos).
    bool anySolo = false;
    for (int i = 1; i < k_busCount; ++i)
    {
        if (m_state[i].solo)
        {
            anySolo = true;
            break;
        }
    }

    // Master = the engine endpoint. Ignores solo (root of the tree); mute still applies.
    const BusState& master = m_state[static_cast<int>(AudioBus::Master)];
    ma_engine_set_volume(m_engine, master.mute ? 0.0f : master.userVolume);

    // Named buses: full mute/solo math.
    for (int i = 1; i < k_busCount; ++i)
        ma_sound_group_set_volume(&m_groups[i - 1], ComputeEffectiveGain(m_state[i], anySolo));
}
