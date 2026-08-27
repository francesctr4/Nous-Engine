#pragma once

#include <VideoSystem/VideoDecoder/IVideoDecoderBackend.h>

#include <cstdint>

// Which decode backend to instantiate. Only FFmpeg exists today; a future hwaccel /
// Media Foundation / GStreamer backend gets an enumerator here.
enum class VideoDecoderBackend : std::int8_t
{
    UNKNOWN = -1,
    FFMPEG  = 0
};

/**
 * @brief Instantiates the concrete video decode backend for the given type.
 *
 * Replaces the old VideoSystem passthrough facade: ModuleVideo now owns an
 * IVideoDecoderBackend* directly and calls this factory to create it. Mirrors
 * CreateRendererBackend, and stays the one TU that names the concrete backend.
 * The returned backend is NOT yet Initialize()d — the caller (ModuleVideo::Awake)
 * does that and owns the lifetime (NOUS_DELETE).
 *
 * @return Heap-allocated backend (NOUS_NEW, MemoryTag::VIDEO_SYSTEM) or nullptr
 *         on an unknown/unsupported type.
 */
[[nodiscard]] IVideoDecoderBackend* CreateVideoDecoderBackend(VideoDecoderBackend type);
