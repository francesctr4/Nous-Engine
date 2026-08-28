#include <EventSystem/Event.h>
#include <Logger/Logger.h>

// ============================================================
// EventContext Helper Implementations
// ============================================================

// -------- [int32_t] --------
EventContext SendContext(const int32_t a)
{
    EventContext ctx; ctx.i32[0] = a;
    NOUS_TRACE("Created int32_t context: %d", a);
    return ctx;
}

EventContext SendContext(const int32_t a, const int32_t b)
{
    EventContext ctx; ctx.i32[0] = a; ctx.i32[1] = b;
    NOUS_TRACE("Created int32x2 context: %d, %d", a, b);
    return ctx;
}

EventContext SendContext(const int32_t a, const int32_t b, const int32_t c)
{
    EventContext ctx; ctx.i32[0] = a; ctx.i32[1] = b; ctx.i32[2] = c;
    NOUS_TRACE("Created int32x3 context: %d, %d, %d", a, b, c);
    return ctx;
}

EventContext SendContext(const int32_t a, const int32_t b, const int32_t c, const int32_t d)
{
    EventContext ctx;
    ctx.i32[0] = a; ctx.i32[1] = b; ctx.i32[2] = c; ctx.i32[3] = d;
    NOUS_TRACE("Created int32x4 context: %d, %d, %d, %d", a, b, c, d);
    return ctx;
}

// -------- [uint32_t] --------
EventContext SendContext(const uint32_t a)
{
    EventContext ctx; ctx.u32[0] = a;
    NOUS_TRACE("Created uint32_t context: %u", a);
    return ctx;
}

EventContext SendContext(const uint32_t a, const uint32_t b)
{
    EventContext ctx; ctx.u32[0] = a; ctx.u32[1] = b;
    NOUS_TRACE("Created uint32x2 context: %u, %u", a, b);
    return ctx;
}

EventContext SendContext(const uint32_t a, const uint32_t b, const uint32_t c)
{
    EventContext ctx; ctx.u32[0] = a; ctx.u32[1] = b; ctx.u32[2] = c;
    NOUS_TRACE("Created uint32x3 context: %u, %u, %u", a, b, c);
    return ctx;
}

// -------- [float] --------
EventContext SendContext(const float a)
{
    EventContext ctx; ctx.f32[0] = a;
    NOUS_TRACE("Created float context: %.3f", a);
    return ctx;
}

EventContext SendContext(const float a, const float b)
{
    EventContext ctx; ctx.f32[0] = a; ctx.f32[1] = b;
    NOUS_TRACE("Created floatx2 context: %.3f, %.3f", a, b);
    return ctx;
}

EventContext SendContext(const float a, const float b, const float c)
{
    EventContext ctx; ctx.f32[0] = a; ctx.f32[1] = b; ctx.f32[2] = c;
    NOUS_TRACE("Created floatx3 context: %.3f, %.3f, %.3f", a, b, c);
    return ctx;
}

// -------- [double] --------
EventContext SendContext(const double a)
{
    EventContext ctx; ctx.f64[0] = a;
    NOUS_TRACE("Created double context: %.3f", a);
    return ctx;
}

EventContext SendContext(const double a, const double b)
{
    EventContext ctx; ctx.f64[0] = a; ctx.f64[1] = b;
    NOUS_TRACE("Created doublex2 context: %.3f, %.3f", a, b);
    return ctx;
}

// -------- [bool] --------
EventContext SendContext(const bool value)
{
    EventContext ctx; ctx.u8[0] = static_cast<uint8_t>(value);
    NOUS_TRACE("Created bool context: %s", value ? "true" : "false");
    return ctx;
}

// -------- [string] --------
EventContext SendContext(const char* str)
{
    EventContext ctx; ctx.c = str;
    NOUS_TRACE("Created string context: \"%s\"", str ? str : "<null>");
    return ctx;
}

// -------- [int64_t] --------
EventContext SendContext(const int64_t a, const int64_t b)
{
    EventContext ctx;
    ctx.i64[0] = a; ctx.i64[1] = b;
    NOUS_TRACE("Created int64x2 context: %lld, %lld", static_cast<long long>(a), static_cast<long long>(b));
    return ctx;
}
