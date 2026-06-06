#pragma once

#include "Engine/Core/Globals.h"
#include "Engine/EngineExport.h"
#include "Engine/Systems/VideoSystem/VideoFrame.h"

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

// Pure selection helper: index of the newest frame whose ptsSeconds[i] <= playheadSec,
// or -1 if none. Assumes ptsSeconds is sorted ascending (true for decoded frames).
// FFmpeg-free; shared by the streamed queue and the predecoded array path.
[[nodiscard]] NOUS_ENGINE_API int SelectNewestFrameIndex(const double* ptsSeconds, uint32 count, double playheadSec);

// Bounded, thread-safe RGBA frame ring. Producer = decoder thread (Push/TryPush);
// consumer = main thread (TryGetForPlayhead). Selection drops frames older than the one
// returned and removes the returned frame, so a frame is delivered at most once.
class VideoFrameQueue
{
public:
    NOUS_ENGINE_API explicit VideoFrameQueue(uint32 capacity);
    NOUS_ENGINE_API ~VideoFrameQueue();

    // Producer. TryPush returns false when full (non-blocking). Push blocks until a slot
    // frees or Stop() is called. Both copy the pixel bytes (width*height*4) into the ring.
    NOUS_ENGINE_API bool TryPush(const uint8_t* rgba, uint32 width, uint32 height, double ptsSec);
    NOUS_ENGINE_API void Push(const uint8_t* rgba, uint32 width, uint32 height, double ptsSec);

    // Consumer. Selects the newest frame with ptsSec <= playheadSec, discards older
    // frames, copies the selected one into an internal latch and points out.pixels at it
    // (valid until the next call). Returns false when no frame is <= playheadSec.
    NOUS_ENGINE_API bool TryGetForPlayhead(double playheadSec, VideoFrame& out);

    NOUS_ENGINE_API void Clear();
    NOUS_ENGINE_API void Stop();   // wake any blocked Push (shutdown)

    [[nodiscard]] NOUS_ENGINE_API uint32 Size() const;
    [[nodiscard]] NOUS_ENGINE_API uint32 Capacity() const;

private:
    struct Slot
    {
        std::vector<uint8_t> rgba;
        uint32               width  = 0;
        uint32               height = 0;
        double               ptsSec = 0.0;
    };

    mutable std::mutex      m_mutex;
    std::condition_variable m_notFull;
    std::deque<Slot>        m_slots;

    std::vector<uint8_t>    m_latch;        // stable buffer backing the last delivered frame
    uint32                  m_latchWidth  = 0;
    uint32                  m_latchHeight = 0;
    double                  m_latchPts    = 0.0;

    uint32                  m_capacity;
    bool                    m_stopped = false;
};
