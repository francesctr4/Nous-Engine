#pragma once

#include <cstdint>

// Pure, backend-agnostic gain application: out[i] = in[i] * gain for every sample
// in an interleaved buffer. No miniaudio — unit-testable headless. The custom gain
// node's process callback is a thin wrapper around this.
inline void GainApply(float* out, const float* in, uint32_t frameCount, uint32_t channels, float gain)
{
    const uint32_t total = frameCount * channels;
    for (uint32_t i = 0; i < total; ++i)
        out[i] = in[i] * gain;
}
