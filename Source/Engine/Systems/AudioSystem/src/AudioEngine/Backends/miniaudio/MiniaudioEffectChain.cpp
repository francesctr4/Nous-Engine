#include "AudioEngine/Backends/miniaudio/MiniaudioEffectChain.h"
#include "AudioEngine/Backends/miniaudio/MiniaudioGainNode.h"

// Vendored from miniaudio's extras/ (downloaded at configure time into
// ThirdParty/MiniaudioNodes) — ma_reverb_node is NOT part of miniaudio.h itself.
// Its definitions are compiled by MiniaudioReverbNode.cpp.
#include <ma_reverb_node.h>

#include <AudioSystem/AudioGraph/AudioEffectRegistry.h>
#include <Logger/LogChannel.h>
#include <Logger/Logger.h>
#include <MemoryManager/MemoryManager.h>
#include <cstddef>

namespace
{
constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_AUDIOSYSTEM;
}

bool MiniaudioEffectChain::Build(ma_engine* engine, ma_node* source, const AudioGraphDesc& desc, ma_node* output)
{
    if (!engine || !source || !output || desc.empty())
        return false;

    m_engine     = engine;
    m_source     = source;
    m_output     = output;
    m_channels   = ma_engine_get_channels(engine);
    m_sampleRate = ma_engine_get_sample_rate(engine);

    ma_node_graph* graph = ma_engine_get_node_graph(engine);

    for (const AudioEffectDesc& e : desc)
    {
        ma_node* node = CreateNode(graph, e);
        if (!node)
        {
            // Without this the whole chain silently fails to build and the voice
            // just plays dry — indistinguishable from a mis-wired graph. The channel
            // count is in the message because it is the known rejection cause:
            // Reverb (verblib) accepts 1 or 2 channels ONLY, and the engine takes its
            // count from the output device, so a surround-configured device fails here
            // on a machine where stereo works.
            NOUS_ERROR_C(CURRENT_CHANNEL,
                         "Effect chain build failed: '%s' node would not initialize "
                         "(engine channels: %d, sample rate: %d). Voice stays dry.",
                         nous::audio::DisplayName(e.type),
                         static_cast<int>(m_channels),
                         static_cast<int>(m_sampleRate));
            CleanupNodes();   // source untouched — still on its bus
            return false;
        }
        m_nodes.push_back({ e.type, node });
    }

    // Wire: source → node[0] → ... → node[N-1] → output.
    ma_node_attach_output_bus(m_source, 0, m_nodes.front().node, 0);
    for (size_t i = 0; i + 1 < m_nodes.size(); ++i)
        ma_node_attach_output_bus(m_nodes[i].node, 0, m_nodes[i + 1].node, 0);
    ma_node_attach_output_bus(m_nodes.back().node, 0, m_output, 0);
    return true;
}

void MiniaudioEffectChain::Destroy() noexcept
{
    // Reattach the source straight to the output first so it keeps producing, then
    // uninit the effect nodes (uninit waits for the audio thread + detaches buses).
    if (m_source && m_output)
        ma_node_attach_output_bus(m_source, 0, m_output, 0);
    CleanupNodes();
}

void MiniaudioEffectChain::SetParam(int effectIndex, int paramIndex, float value)
{
    if (effectIndex < 0 || effectIndex >= static_cast<int>(m_nodes.size()))
        return;

    const ChainNode& cn = m_nodes[static_cast<size_t>(effectIndex)];
    switch (cn.type)
    {
        case AudioEffectType::LowPass:
            if (paramIndex == 0)
            {
                ma_lpf_config c = ma_lpf_config_init(ma_format_f32, m_channels, m_sampleRate, value, 2);
                ma_lpf_node_reinit(&c, reinterpret_cast<ma_lpf_node*>(cn.node));
            }
            break;
        case AudioEffectType::HighPass:
            if (paramIndex == 0)
            {
                ma_hpf_config c = ma_hpf_config_init(ma_format_f32, m_channels, m_sampleRate, value, 2);
                ma_hpf_node_reinit(&c, reinterpret_cast<ma_hpf_node*>(cn.node));
            }
            break;
        case AudioEffectType::Delay:
            // paramIndex 0 (delay time) is fixed at init (buffer size) — a delay-time
            // change is applied by a full chain rebuild (generation bump). 1=decay, 2=wet.
            if      (paramIndex == 1) ma_delay_node_set_decay(reinterpret_cast<ma_delay_node*>(cn.node), value);
            else if (paramIndex == 2) ma_delay_node_set_wet  (reinterpret_cast<ma_delay_node*>(cn.node), value);
            break;
        case AudioEffectType::Gain:
            if (paramIndex == 0)
                reinterpret_cast<MiniaudioGainNode*>(cn.node)->gain = value;
            break;
        case AudioEffectType::Reverb:
        {
            // Unlike Delay, EVERY reverb param is live — verblib recomputes its comb
            // and allpass coefficients in place, so no param here forces a rebuild.
            verblib* v = &reinterpret_cast<ma_reverb_node*>(cn.node)->reverb;
            switch (paramIndex)
            {
                case 0: verblib_set_room_size(v, value); break;
                case 1: verblib_set_damping  (v, value); break;
                case 2: verblib_set_wet      (v, value); break;
                case 3: verblib_set_dry      (v, value); break;
                case 4: verblib_set_width    (v, value); break;
                default: break;
            }
            break;
        }
        case AudioEffectType::COUNT:
            break;   // not an effect
    }
}

ma_node* MiniaudioEffectChain::CreateNode(ma_node_graph* graph, const AudioEffectDesc& d)
{
    switch (d.type)
    {
        case AudioEffectType::LowPass:
        {
            const double cutoff = d.params.empty() ? 1000.0 : static_cast<double>(d.params[0]);
            ma_lpf_node_config cfg = ma_lpf_node_config_init(m_channels, m_sampleRate, cutoff, 2);
            auto* n = NOUS_NEW<ma_lpf_node>(MemoryTag::AUDIO_SYSTEM);
            if (ma_lpf_node_init(graph, &cfg, nullptr, n) != MA_SUCCESS)
            {
                NOUS_DELETE(n, MemoryTag::AUDIO_SYSTEM);
                return nullptr;
            }
            return reinterpret_cast<ma_node*>(n);
        }
        case AudioEffectType::HighPass:
        {
            const double cutoff = d.params.empty() ? 200.0 : static_cast<double>(d.params[0]);
            ma_hpf_node_config cfg = ma_hpf_node_config_init(m_channels, m_sampleRate, cutoff, 2);
            auto* n = NOUS_NEW<ma_hpf_node>(MemoryTag::AUDIO_SYSTEM);
            if (ma_hpf_node_init(graph, &cfg, nullptr, n) != MA_SUCCESS)
            {
                NOUS_DELETE(n, MemoryTag::AUDIO_SYSTEM);
                return nullptr;
            }
            return reinterpret_cast<ma_node*>(n);
        }
        case AudioEffectType::Delay:
        {
            const float delaySec = d.params.size() > 0 ? d.params[0] : 0.25f;
            const float decay    = d.params.size() > 1 ? d.params[1] : 0.40f;
            const float wet      = d.params.size() > 2 ? d.params[2] : 0.50f;
            ma_uint32 delayFrames = static_cast<ma_uint32>(delaySec * static_cast<float>(m_sampleRate));
            if (delayFrames == 0) delayFrames = 1;  // 0-length delay buffer is invalid
            ma_delay_node_config cfg = ma_delay_node_config_init(m_channels, m_sampleRate, delayFrames, decay);
            auto* n = NOUS_NEW<ma_delay_node>(MemoryTag::AUDIO_SYSTEM);
            if (ma_delay_node_init(graph, &cfg, nullptr, n) != MA_SUCCESS)
            {
                NOUS_DELETE(n, MemoryTag::AUDIO_SYSTEM);
                return nullptr;
            }
            ma_delay_node_set_wet(n, wet);
            ma_delay_node_set_dry(n, 1.0f);  // keep the original signal (echo, not full-wet)
            return reinterpret_cast<ma_node*>(n);
        }
        case AudioEffectType::Gain:
        {
            const float gain = d.params.empty() ? 1.0f : d.params[0];
            auto* n = NOUS_NEW<MiniaudioGainNode>(MemoryTag::AUDIO_SYSTEM);
            if (MiniaudioGainNodeInit(graph, m_channels, gain, n) != MA_SUCCESS)
            {
                NOUS_DELETE(n, MemoryTag::AUDIO_SYSTEM);
                return nullptr;
            }
            return reinterpret_cast<ma_node*>(n);
        }
        case AudioEffectType::Reverb:
        {
            // verblib handles 1 or 2 channels only; ma_reverb_node_init fails cleanly
            // on anything else, which is the behaviour we want (no silent passthrough).
            ma_reverb_node_config cfg = ma_reverb_node_config_init(m_channels, m_sampleRate);
            if (d.params.size() > 0) cfg.roomSize  = d.params[0];
            if (d.params.size() > 1) cfg.damping   = d.params[1];
            if (d.params.size() > 2) cfg.wetVolume = d.params[2];
            if (d.params.size() > 3) cfg.dryVolume = d.params[3];
            if (d.params.size() > 4) cfg.width     = d.params[4];

            auto* n = NOUS_NEW<ma_reverb_node>(MemoryTag::AUDIO_SYSTEM);
            if (ma_reverb_node_init(graph, &cfg, nullptr, n) != MA_SUCCESS)
            {
                NOUS_DELETE(n, MemoryTag::AUDIO_SYSTEM);
                return nullptr;
            }
            return reinterpret_cast<ma_node*>(n);
        }
        case AudioEffectType::COUNT:
            break;   // not an effect
    }
    return nullptr;
}

void MiniaudioEffectChain::DestroyNode(const ChainNode& cn) noexcept
{
    switch (cn.type)
    {
        case AudioEffectType::LowPass:
        {
            auto* n = reinterpret_cast<ma_lpf_node*>(cn.node);
            ma_lpf_node_uninit(n, nullptr);
            NOUS_DELETE(n, MemoryTag::AUDIO_SYSTEM);
            break;
        }
        case AudioEffectType::HighPass:
        {
            auto* n = reinterpret_cast<ma_hpf_node*>(cn.node);
            ma_hpf_node_uninit(n, nullptr);
            NOUS_DELETE(n, MemoryTag::AUDIO_SYSTEM);
            break;
        }
        case AudioEffectType::Delay:
        {
            auto* n = reinterpret_cast<ma_delay_node*>(cn.node);
            ma_delay_node_uninit(n, nullptr);
            NOUS_DELETE(n, MemoryTag::AUDIO_SYSTEM);
            break;
        }
        case AudioEffectType::Gain:
        {
            auto* n = reinterpret_cast<MiniaudioGainNode*>(cn.node);
            MiniaudioGainNodeUninit(n);
            NOUS_DELETE(n, MemoryTag::AUDIO_SYSTEM);
            break;
        }
        case AudioEffectType::Reverb:
        {
            auto* n = reinterpret_cast<ma_reverb_node*>(cn.node);
            ma_reverb_node_uninit(n, nullptr);
            NOUS_DELETE(n, MemoryTag::AUDIO_SYSTEM);
            break;
        }
        case AudioEffectType::COUNT:
            break;   // not an effect
    }
}

void MiniaudioEffectChain::CleanupNodes() noexcept
{
    for (const ChainNode& cn : m_nodes)
        DestroyNode(cn);
    m_nodes.clear();
}
