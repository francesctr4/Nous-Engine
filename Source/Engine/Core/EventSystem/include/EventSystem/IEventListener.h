#ifndef NOUS_ENGINE_IEVENTLISTENER_H
#define NOUS_ENGINE_IEVENTLISTENER_H

#include <EventSystem/Event.h>

class IEventListener
{
public:
    virtual ~IEventListener() = default;
    virtual void OnEvent(const Event& event) = 0;
};

#endif //NOUS_ENGINE_IEVENTLISTENER_H
