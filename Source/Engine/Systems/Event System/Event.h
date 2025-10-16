#ifndef NOUS_ENGINE_EVENT_H
#define NOUS_ENGINE_EVENT_H

#include <cstdint>
#include <cstring>

enum class EventType : uint8_t
{
    NONE = 0,
    TEST,
    KEY_PRESSED,
    WINDOW_RESIZED,
    SWAP_TEXTURE,
    DROP_FILE,
    INPUT_EVENT,
    IMGUI_RECREATION,
    WINDOW_MINIMIZED,
    KEY_RELEASED,
    MOUSE_BUTTON,
    MOUSE_MOVED,
    FILE_DROPPED,
    ENGINE_SHUTDOWN,
    IMGUI_RECREATE,
    CUSTOM // Extend freely
};

// A flexible union for different data payloads
union EventContext
{
    int64_t   i64[2];
    uint64_t  u64[2];
    double    f64[2];

    int32_t   i32[4];
    uint32_t  u32[4];
    float     f32[4];

    int16_t   i16[8];
    uint16_t  u16[8];

    int8_t    i8[16];
    uint8_t   u8[16];

    const char* c; // string pointer

    EventContext() { std::memset(this, 0, sizeof(EventContext)); }
};

struct Event
{
    EventType type { EventType::NONE };
    EventContext ctx {};

    Event() = default;
    explicit Event(EventType t) : type(t) {}
    Event(EventType t, const EventContext& c) : type(t), ctx(c) {}
};

// ------------------------------------------------------------
// 🧰 Helper Factory Functions for EventContext
// ------------------------------------------------------------

// --------------- Integer helpers ---------------
inline EventContext SendContext(int32_t a)
{
    EventContext ctx;
    ctx.i32[0] = a;
    return ctx;
}

inline EventContext SendContext(int32_t a, int32_t b)
{
    EventContext ctx;
    ctx.i32[0] = a;
    ctx.i32[1] = b;
    return ctx;
}

inline EventContext SendContext(int32_t a, int32_t b, int32_t c)
{
    EventContext ctx;
    ctx.i32[0] = a;
    ctx.i32[1] = b;
    ctx.i32[2] = c;
    return ctx;
}

inline EventContext SendContext(int32_t a, int32_t b, int32_t c, int32_t d)
{
    EventContext ctx;
    ctx.i32[0] = a;
    ctx.i32[1] = b;
    ctx.i32[2] = c;
    ctx.i32[3] = d;
    return ctx;
}

// --------------- Unsigned Integer helpers ---------------
inline EventContext SendContext(uint32_t a)
{
    EventContext ctx;
    ctx.u32[0] = a;
    return ctx;
}

inline EventContext SendContext(uint32_t a, uint32_t b)
{
    EventContext ctx;
    ctx.u32[0] = a;
    ctx.u32[1] = b;
    return ctx;
}

inline EventContext SendContext(uint32_t a, uint32_t b, uint32_t c)
{
    EventContext ctx;
    ctx.u32[0] = a;
    ctx.u32[1] = b;
    ctx.u32[2] = c;
    return ctx;
}

// --------------- Floating Point helpers ---------------
inline EventContext SendContext(float a)
{
    EventContext ctx;
    ctx.f32[0] = a;
    return ctx;
}

inline EventContext SendContext(float a, float b)
{
    EventContext ctx;
    ctx.f32[0] = a;
    ctx.f32[1] = b;
    return ctx;
}

inline EventContext SendContext(float a, float b, float c)
{
    EventContext ctx;
    ctx.f32[0] = a;
    ctx.f32[1] = b;
    ctx.f32[2] = c;
    return ctx;
}

// --------------- Double helpers ---------------
inline EventContext SendContext(double a)
{
    EventContext ctx;
    ctx.f64[0] = a;
    return ctx;
}

inline EventContext SendContext(double a, double b)
{
    EventContext ctx;
    ctx.f64[0] = a;
    ctx.f64[1] = b;
    return ctx;
}

// --------------- Boolean helper ---------------
inline EventContext SendContext(bool value)
{
    EventContext ctx;
    ctx.u8[0] = static_cast<uint8_t>(value);
    return ctx;
}

// --------------- String helper ---------------
inline EventContext SendContext(const char* str)
{
    EventContext ctx;
    ctx.c = str;
    return ctx;
}

// --------------- Pointer helper (generic) ---------------
inline EventContext SendContext(void* ptr)
{
    EventContext ctx;
    ctx.u64[0] = reinterpret_cast<uint64_t>(ptr);
    return ctx;
}

// --------------- 64-bit integer helper ---------------
inline EventContext SendContext(int64_t a, int64_t b = 0)
{
    EventContext ctx;
    ctx.i64[0] = a;
    ctx.i64[1] = b;
    return ctx;
}

#endif //NOUS_ENGINE_EVENT_H
