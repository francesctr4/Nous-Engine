#pragma once

// Opaque handle to a backend-owned decoder instance (one per CVideoPlayer / playhead).
// Callers treat it as a token — only the concrete decode backend knows it is a
// heap-allocated FFmpeg decode context. Lives in its own dependency-free header (not in
// IVideoDecoderBackend.h) so components and modules can name the type without depending
// on the backend abstraction. A null handle means "no video".
using VideoHandle = void*;
