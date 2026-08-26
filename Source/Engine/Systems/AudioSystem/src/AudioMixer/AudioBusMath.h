#pragma once

// Dependency-free mixer math — no miniaudio, no engine deps — so it is unit-tested
// headless (mirrors t_VideoPlayhead). This is the backend-agnostic bus policy: a
// backend's bus graph (e.g. MiniaudioBusGraph) stores one BusState per bus and pushes
// ComputeEffectiveGain() into its mixer whenever any bus state changes.

struct BusState
{
    float userVolume = 1.0f;  // linear gain authored by the mixer UI (1.0 = unity)
    bool  mute       = false;
    bool  solo       = false;
};

// Effective linear gain for one bus, given whether ANY (named) bus is soloed.
// Mute always wins. When something is soloed, non-soloed buses are silenced.
inline float ComputeEffectiveGain(const BusState& bus, bool anySolo)
{
    if (bus.mute)             return 0.0f;
    if (anySolo && !bus.solo) return 0.0f;
    return bus.userVolume;
}
