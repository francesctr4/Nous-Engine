#ifndef NOUS_ENGINE_EVENTSYSTEM_H
#define NOUS_ENGINE_EVENTSYSTEM_H

#include "Event.h"
#include "IEventListener.h"

#include <unordered_map>
#include <vector>
#include <queue>
#include <mutex>
#include <algorithm>

class EventSystem
{
public:
    EventSystem() = default;
    ~EventSystem() = default;

    // Prevent copying
    EventSystem(const EventSystem&) = delete;
    EventSystem& operator=(const EventSystem&) = delete;

    // ------------------------------------------
    // 📥 Subscription Management
    // ------------------------------------------
    void Subscribe(EventType type, IEventListener* listener)
    {
        std::scoped_lock lock(m_Mutex);
        m_Listeners[type].push_back(listener);
    }

    void Unsubscribe(EventType type, IEventListener* listener)
    {
        std::scoped_lock lock{};
        auto& vec = m_Listeners[type];
        vec.erase(std::remove(vec.begin(), vec.end(), listener), vec.end());
    }

    // ------------------------------------------
    // 🚀 Event Broadcasting
    // ------------------------------------------
    // Immediate (synchronous)
    void Broadcast(const Event& evt)
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
        }
    }

    // Queued (asynchronous / deferred)
    void Queue(const Event& evt)
    {
        std::scoped_lock lock(m_Mutex);
        m_Queue.push(evt);
    }

    // ------------------------------------------
    // 🌀 Dispatch queued events (called each frame)
    // ------------------------------------------
    void DispatchQueued()
    {
        std::queue<Event> tempQueue;

        { // swap under lock
            std::scoped_lock lock(m_Mutex);
            std::swap(tempQueue, m_Queue);
        }

        while (!tempQueue.empty())
        {
            Broadcast(tempQueue.front());
            tempQueue.pop();
        }
    }

    // ------------------------------------------
    // 🔧 Utilities
    // ------------------------------------------
    void Clear()
    {
        std::scoped_lock lock(m_Mutex);
        m_Listeners.clear();
        while (!m_Queue.empty()) m_Queue.pop();
    }

private:
    std::unordered_map<EventType, std::vector<IEventListener*>> m_Listeners;
    std::queue<Event> m_Queue;
    std::mutex m_Mutex;
};

#endif //NOUS_ENGINE_EVENTSYSTEM_H