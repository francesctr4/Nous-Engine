#pragma once

#include <VideoSystem/VideoFrame.h>

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <mutex>
#include <vector>

// Pure selection helper: index of the newest frame whose ptsSeconds[i] <= playheadSec,
// or -1 if none. Assumes ptsSeconds is sorted ascending (true for decoded frames).
// FFmpeg-free; shared by the streamed queue and the predecoded array path.
[[nodiscard]] int SelectNewestFrameIndex(const double* ptsSeconds, uint32_t count, double playheadSec);

// Bounded, thread-safe RGBA frame ring. Producer = decoder thread (Push/TryPush);
// consumer = main thread (TryGetForPlayhead). Selection drops frames older than the one
// returned and removes the returned frame, so a frame is delivered at most once.
class VideoFrameQueue
{
public:
    explicit VideoFrameQueue(uint32_t capacity);
    ~VideoFrameQueue();

    // Producer. TryPush returns false when full (non-blocking). Push blocks until a slot
    // frees or Stop() is called. Both copy the pixel bytes (width*height*4) into the ring.
    bool TryPush(const uint8_t* rgba, uint32_t width, uint32_t height, double ptsSec);
    void Push(const uint8_t* rgba, uint32_t width, uint32_t height, double ptsSec);

    // Consumer. Selects the newest frame with ptsSec <= playheadSec, discards older
    // frames, copies the selected one into an internal latch and points out.pixels at it
    // (valid until the next call). Returns false when no frame is <= playheadSec.
    bool TryGetForPlayhead(double playheadSec, VideoFrame& out);

    void Clear();
    void Stop();   // wake any blocked Push (shutdown)

    [[nodiscard]] uint32_t Size() const;
    [[nodiscard]] uint32_t Capacity() const;

private:
    struct Slot
    {
        std::vector<uint8_t> rgba;
        uint32_t               width  = 0;
        uint32_t               height = 0;
        double               ptsSec = 0.0;
    };

    mutable std::mutex      m_mutex;
    std::condition_variable m_notFull;
    std::deque<Slot>        m_slots;

    std::vector<uint8_t>    m_latch;        // stable buffer backing the last delivered frame
    uint32_t                  m_latchWidth  = 0;
    uint32_t                  m_latchHeight = 0;
    double                  m_latchPts    = 0.0;

    uint32_t                  m_capacity;
    bool                    m_stopped = false;
};
