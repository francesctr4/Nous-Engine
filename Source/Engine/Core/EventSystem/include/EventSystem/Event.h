#ifndef NOUS_ENGINE_EVENT_H
#define NOUS_ENGINE_EVENT_H

#include <cstdint>
#include <cstring>

// ------------------------------------------------------------
// Event Type Enumeration
// ------------------------------------------------------------
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

// ------------------------------------------------------------
// Event Context - Generic Payload Union
// ------------------------------------------------------------
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
    void* ptr[2];

    EventContext() : u64{0,0} { std::memset(this, 0, sizeof(EventContext)); }
};

// ------------------------------------------------------------
// Event Structure
// ------------------------------------------------------------
struct Event
{
    EventType type { EventType::NONE };
    EventContext ctx {};

    Event() = default;
    explicit Event(const EventType t) : type(t) {}
    Event(const EventType t, const EventContext& c) : type(t), ctx(c) {}
};

// ------------------------------------------------------------
// EventContext Helper Functions (declarations)
// ------------------------------------------------------------

// Integer helpers
EventContext SendContext(int32_t a);
EventContext SendContext(int32_t a, int32_t b);
EventContext SendContext(int32_t a, int32_t b, int32_t c);
EventContext SendContext(int32_t a, int32_t b, int32_t c, int32_t d);

// Unsigned integer helpers
EventContext SendContext(uint32_t a);
EventContext SendContext(uint32_t a, uint32_t b);
EventContext SendContext(uint32_t a, uint32_t b, uint32_t c);

// Floating point helpers
EventContext SendContext(float a);
EventContext SendContext(float a, float b);
EventContext SendContext(float a, float b, float c);

// Double helpers
EventContext SendContext(double a);
EventContext SendContext(double a, double b);

// Boolean, string, pointer, and 64-bit helpers
EventContext SendContext(bool value);
EventContext SendContext(const char* str);
EventContext SendContext(int64_t a, int64_t b = 0);

// -------- [pointer] --------
template<typename T>
EventContext SendContext(T* ptr)
{
    EventContext ctx;
    ctx.ptr[0] = static_cast<void*>(ptr);
    return ctx;
}

#endif // NOUS_ENGINE_EVENT_H
