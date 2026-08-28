#pragma once

#include <cstdint>

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

    virtual int32_t GetMouseXMotion() const = 0;
    virtual int32_t GetMouseYMotion() const = 0;
    virtual int32_t GetMouseZ() const = 0;
};

