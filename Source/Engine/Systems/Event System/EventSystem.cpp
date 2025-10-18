#include "EventSystem.h"
#include "Engine/Utils/Logging System/Logger.h"   // uses your NOUS_INFO / NOUS_WARN macros
#include <algorithm>

constexpr LogChannel CURRENT_CHANNEL = LogChannel::NOUSENGINE_SYSTEMS_EVENTSYSTEM;

// --------------------------------------------------
// Constructor / Destructor
// --------------------------------------------------
EventSystem::EventSystem()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "[%s] Initialized", __FUNCTION__);
}

EventSystem::~EventSystem()
{
    Clear();
    NOUS_INFO("[%s] Shutdown and cleared all listeners", __FUNCTION__);
}

// --------------------------------------------------
// Subscription Management
// --------------------------------------------------
void EventSystem::Subscribe(EventType type, IEventListener* listener)
{
    std::scoped_lock lock(m_Mutex);

    auto& list = m_Listeners[type];
    if (std::find(list.begin(), list.end(), listener) == list.end())
    {
        list.push_back(listener);
        NOUS_INFO("[%s] Listener subscribed to event type: %d", __FUNCTION__, static_cast<int>(type));
    }
    else
    {
        NOUS_WARN("[%s] Attempted to subscribe duplicate listener to event type: %d",
                  __FUNCTION__, static_cast<int>(type));
    }
}

void EventSystem::Unsubscribe(EventType type, IEventListener* listener)
{
    std::scoped_lock lock(m_Mutex);

    auto it = m_Listeners.find(type);
    if (it != m_Listeners.end())
    {
        auto& vec = it->second;
        vec.erase(std::remove(vec.begin(), vec.end(), listener), vec.end());
        NOUS_INFO("[%s] Listener unsubscribed from event type: %d", __FUNCTION__, static_cast<int>(type));
    }
    else
    {
        NOUS_WARN("[%s] Tried to unsubscribe listener from unregistered event type: %d",
                  __FUNCTION__, static_cast<int>(type));
    }
}

// --------------------------------------------------
// Event Broadcasting
// --------------------------------------------------
void EventSystem::Broadcast(const Event& evt)
{
    std::scoped_lock lock(m_Mutex);

    auto it = m_Listeners.find(evt.type);
    if (it != m_Listeners.end())
    {
        for (IEventListener* listener : it->second)
        {
            if (listener)
                listener->OnEvent(evt);
        }

        NOUS_TRACE("[%s] Broadcasted event type: %d to %zu listener(s)", __FUNCTION__,
                   static_cast<int>(evt.type), it->second.size());
    }
    else
    {
        NOUS_WARN("[%s] Broadcasted event type: %d (no listeners registered)", __FUNCTION__,
                   static_cast<int>(evt.type));
    }
}

// --------------------------------------------------
// Queued Events
// --------------------------------------------------
void EventSystem::Queue(const Event& evt)
{
    std::scoped_lock lock(m_Mutex);
    m_Queue.push(evt);
    NOUS_INFO("[%s] Queued event type: %d", __FUNCTION__, static_cast<int>(evt.type));
}

// --------------------------------------------------
// Dispatch queued events
// --------------------------------------------------
void EventSystem::DispatchQueued()
{
    std::queue<Event> tempQueue;

    {   // Lock only while swapping to minimize contention
        std::scoped_lock lock(m_Mutex);
        std::swap(tempQueue, m_Queue);
    }

    if (!tempQueue.empty())
        NOUS_DEBUG("[%s] Dispatching %zu queued event(s)", __FUNCTION__, tempQueue.size());

    while (!tempQueue.empty())
    {
        Broadcast(tempQueue.front());
        tempQueue.pop();
    }
}

// --------------------------------------------------
// Clear all listeners and queued events
// --------------------------------------------------
void EventSystem::Clear()
{
    std::scoped_lock lock(m_Mutex);

    size_t listenerCount = 0;
    for (const auto& [_, vec] : m_Listeners)
        listenerCount += vec.size();

    m_Listeners.clear();
    while (!m_Queue.empty()) m_Queue.pop();

    NOUS_DEBUG("[%s] Cleared %zu listeners and emptied event queue", __FUNCTION__, listenerCount);
}
