#pragma once

#include <EngineCore/Globals.h>
#include <cstdint>

// A single decoded video frame handed across the decode seam. The `pixels` buffer is
// owned by the backend and stays valid only until the next TryGetFrame / DestroyVideo on
// the same handle (the "latched pointer" contract) — callers must not stash it. The
// struct (vs a raw pointer) is the deliberate extension point for a future GPU-side YUV
// path (add a format + extra plane pointers without redesigning the seam).
struct VideoFrame
{
    const uint8_t* pixels = nullptr;   // tightly-packed RGBA8 (MVP)
    uint32         width  = 0;
    uint32         height = 0;
    double         ptsSec = 0.0;
};
