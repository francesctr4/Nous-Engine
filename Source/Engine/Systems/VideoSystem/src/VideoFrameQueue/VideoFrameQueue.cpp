#include "VideoFrameQueue/VideoFrameQueue.h"

#include <algorithm>
#include <cstddef>

int SelectNewestFrameIndex(const double* ptsSeconds, uint32_t count, double playheadSec)
{
    int idx = -1;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (ptsSeconds[i] <= playheadSec) idx = static_cast<int>(i);
        else                              break;   // ascending: no later frame qualifies
    }
    return idx;
}

VideoFrameQueue::VideoFrameQueue(const uint32_t capacity)
    : m_capacity(capacity == 0 ? 1 : capacity)
{
}

VideoFrameQueue::~VideoFrameQueue() = default;

bool VideoFrameQueue::TryPush(const uint8_t* rgba, const uint32_t width, const uint32_t height, const double ptsSec)
{
    std::lock_guard lock(m_mutex);
    if (m_slots.size() >= m_capacity) return false;

    Slot s;
    s.width  = width;
    s.height = height;
    s.ptsSec = ptsSec;
    s.rgba.assign(rgba, rgba + static_cast<size_t>(width) * height * 4);
    m_slots.push_back(std::move(s));
    return true;
}

void VideoFrameQueue::Push(const uint8_t* rgba, const uint32_t width, const uint32_t height, const double ptsSec)
{
    std::unique_lock lock(m_mutex);
    m_notFull.wait(lock, [this] { return m_stopped || m_slots.size() < m_capacity; });
    if (m_stopped) return;

    Slot s;
    s.width  = width;
    s.height = height;
    s.ptsSec = ptsSec;
    s.rgba.assign(rgba, rgba + static_cast<size_t>(width) * height * 4);
    m_slots.push_back(std::move(s));
}

bool VideoFrameQueue::TryGetForPlayhead(const double playheadSec, VideoFrame& out)
{
    std::lock_guard lock(m_mutex);

    bool found = false;
    while (!m_slots.empty() && m_slots.front().ptsSec <= playheadSec)
    {
        Slot s = std::move(m_slots.front());
        m_slots.pop_front();
        m_latch       = std::move(s.rgba);   // keep latest qualifying frame; older ones discarded
        m_latchWidth  = s.width;
        m_latchHeight = s.height;
        m_latchPts    = s.ptsSec;
        found = true;
    }

    if (!found) return false;

    m_notFull.notify_all();   // freed at least one slot
    out.pixels = m_latch.data();
    out.width  = m_latchWidth;
    out.height = m_latchHeight;
    out.ptsSec = m_latchPts;
    return true;
}

void VideoFrameQueue::Clear()
{
    std::lock_guard lock(m_mutex);
    m_slots.clear();
    m_notFull.notify_all();
}

void VideoFrameQueue::Stop()
{
    std::lock_guard lock(m_mutex);
    m_stopped = true;
    m_notFull.notify_all();
}

uint32_t VideoFrameQueue::Size() const
{
    std::lock_guard lock(m_mutex);
    return static_cast<uint32_t>(m_slots.size());
}

uint32_t VideoFrameQueue::Capacity() const
{
    std::lock_guard lock(m_mutex);
    return m_capacity;
}
