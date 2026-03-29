#ifndef NOUS_ENGINE_EVENTSYSTEM_H
#define NOUS_ENGINE_EVENTSYSTEM_H

#include "Event/include/Event.h"
#include "IEventListener.h"
#include "Engine/EngineExport.h"

#include <unordered_map>
#include <vector>
#include <queue>
#include <mutex>

// Forward declaration for logging
class Logger;

class EventSystem
{
public:
    NOUS_ENGINE_API EventSystem();
    NOUS_ENGINE_API ~EventSystem();

    // Non-copyable
    EventSystem(const EventSystem&) = delete;
    EventSystem& operator=(const EventSystem&) = delete;

    // --------------------------------------------------
    // Subscription Management
    // --------------------------------------------------
    NOUS_ENGINE_API void Subscribe(EventType type, IEventListener* listener);
    NOUS_ENGINE_API void Unsubscribe(EventType type, IEventListener* listener);

    // --------------------------------------------------
    // Event Broadcasting
    // --------------------------------------------------
    // Immediate (synchronous)
    NOUS_ENGINE_API void Broadcast(const Event& evt);

    // Queued (deferred)
    NOUS_ENGINE_API void Queue(const Event& evt);

    // --------------------------------------------------
    // Dispatch queued events (call each frame)
    // --------------------------------------------------
    void DispatchQueued();

    // --------------------------------------------------
    // Utilities
    // --------------------------------------------------
    void Clear();

private:
    std::unordered_map<EventType, std::vector<IEventListener*>> m_Listeners;
    std::queue<Event> m_Queue;
    std::mutex m_Mutex;
};

#endif // NOUS_ENGINE_EVENTSYSTEM_H