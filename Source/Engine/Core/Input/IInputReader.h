#ifndef NOUS_IINPUTREADER_H
#define NOUS_IINPUTREADER_H

#include "Engine/Core/Globals.h"

enum class KeyState : uint8_t
{
    IDLE,
    DOWN,
    REPEAT,
    UP
};

class IInputReader
{
public:
    virtual ~IInputReader() = default;

    virtual KeyState GetKey(int id) const = 0;
    virtual KeyState GetMouseButton(int id) const = 0;

    virtual int32 GetMouseXMotion() const = 0;
    virtual int32 GetMouseYMotion() const = 0;
    virtual int32 GetMouseZ() const = 0;
};

#endif // NOUS_IINPUTREADER_H
