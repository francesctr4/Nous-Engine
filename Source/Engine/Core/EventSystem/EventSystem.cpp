#include "EventSystem.h"
#include "Engine/Core/Logger/Logger.h"   // uses your NOUS_INFO / NOUS_WARN macros
#include <algorithm>
#include <ranges>

constexpr auto CURRENT_CHANNEL = LogChannel::NOUS_ENGINE_SYSTEM_EVENTSYSTEM;

// --------------------------------------------------
// Constructor / Destructor
// --------------------------------------------------
EventSystem::EventSystem()
{
    NOUS_INFO_C(CURRENT_CHANNEL, "Initialized");
}

EventSystem::~EventSystem()
{
    Clear();
    NOUS_INFO("Shutdown and cleared all listeners");
}

// --------------------------------------------------
// Subscription Management
// --------------------------------------------------
void EventSystem::Subscribe(EventType type, IEventListener* listener)
{
    std::scoped_lock lock(m_Mutex);

    if (auto& list = m_Listeners[type]; std::ranges::find(list, listener) == list.end())
    {
        list.push_back(listener);
        NOUS_INFO("Listener subscribed to event type: %d", static_cast<int>(type));
    }
    else
    {
        NOUS_WARN("Attempted to subscribe duplicate listener to event type: %d", static_cast<int>(type));
    }
}

void EventSystem::Unsubscribe(EventType type, IEventListener* listener)
{
    std::scoped_lock lock(m_Mutex);

    if (const auto it = m_Listeners.find(type); it != m_Listeners.end())
    {
        auto& vec = it->second;
        std::erase(vec, listener);
        NOUS_INFO("Listener unsubscribed from event type: %d", static_cast<int>(type));
    }
    else
    {
        NOUS_WARN("Tried to unsubscribe listener from unregistered event type: %d", static_cast<int>(type));
    }
}

// --------------------------------------------------
// Event Broadcasting
// --------------------------------------------------
void EventSystem::Broadcast(const Event& evt)
{
    std::scoped_lock lock(m_Mutex);

    if (const auto it = m_Listeners.find(evt.type); it != m_Listeners.end())
    {
        for (IEventListener* listener : it->second)
        {
            if (listener)
                listener->OnEvent(evt);
        }
    }
}

// --------------------------------------------------
// Queued Events
// --------------------------------------------------
void EventSystem::Queue(const Event& evt)
{
    std::scoped_lock lock(m_Mutex);
    m_Queue.push(evt);
    NOUS_INFO("Queued event type: %d", static_cast<int>(evt.type));
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
        NOUS_DEBUG("Dispatching %zu queued event(s)", tempQueue.size());

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
    for (const auto& vec : m_Listeners | std::views::values)
        listenerCount += vec.size();

    m_Listeners.clear();
    while (!m_Queue.empty()) m_Queue.pop();

    NOUS_DEBUG("Cleared %zu listeners and emptied event queue", listenerCount);
}
