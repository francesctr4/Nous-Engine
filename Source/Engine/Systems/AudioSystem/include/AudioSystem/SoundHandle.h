#pragma once

// Opaque handle to a backend-owned playing voice (one per CAudioSource instance).
// Callers treat it as a token — only the concrete audio backend knows it is a
// heap-allocated ma_sound.
//
// Lives in its own dependency-free header (not in IAudioEngineBackend.h) so that
// components and modules can name the type without depending on the backend
// abstraction. A null handle means "no voice".
using SoundHandle = void*;
