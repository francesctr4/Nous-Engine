#include "Engine/Core/EventSystem/Event/include/Event.h"
#include "Engine/Core/LoggingSystem/Logger.h"

// ============================================================
// EventContext Helper Implementations
// ============================================================

// -------- [int32] --------
EventContext SendContext(int32_t a)
{
    EventContext ctx; ctx.i32[0] = a;
    NOUS_TRACE("[%s] Created int32 context: %d", __FUNCTION__, a);
    return ctx;
}

EventContext SendContext(int32_t a, int32_t b)
{
    EventContext ctx; ctx.i32[0] = a; ctx.i32[1] = b;
    NOUS_TRACE("[%s] Created int32x2 context: %d, %d", __FUNCTION__, a, b);
    return ctx;
}

EventContext SendContext(int32_t a, int32_t b, int32_t c)
{
    EventContext ctx; ctx.i32[0] = a; ctx.i32[1] = b; ctx.i32[2] = c;
    NOUS_TRACE("[%s] Created int32x3 context: %d, %d, %d", __FUNCTION__, a, b, c);
    return ctx;
}

EventContext SendContext(int32_t a, int32_t b, int32_t c, int32_t d)
{
    EventContext ctx;
    ctx.i32[0] = a; ctx.i32[1] = b; ctx.i32[2] = c; ctx.i32[3] = d;
    NOUS_TRACE("[%s] Created int32x4 context: %d, %d, %d, %d", __FUNCTION__, a, b, c, d);
    return ctx;
}

// -------- [uint32] --------
EventContext SendContext(uint32_t a)
{
    EventContext ctx; ctx.u32[0] = a;
    NOUS_TRACE("[%s] Created uint32 context: %u", __FUNCTION__, a);
    return ctx;
}

EventContext SendContext(uint32_t a, uint32_t b)
{
    EventContext ctx; ctx.u32[0] = a; ctx.u32[1] = b;
    NOUS_TRACE("[%s] Created uint32x2 context: %u, %u", __FUNCTION__, a, b);
    return ctx;
}

EventContext SendContext(uint32_t a, uint32_t b, uint32_t c)
{
    EventContext ctx; ctx.u32[0] = a; ctx.u32[1] = b; ctx.u32[2] = c;
    NOUS_TRACE("[%s] Created uint32x3 context: %u, %u, %u", __FUNCTION__, a, b, c);
    return ctx;
}

// -------- [float] --------
EventContext SendContext(float a)
{
    EventContext ctx; ctx.f32[0] = a;
    NOUS_TRACE("[%s] Created float context: %.3f", __FUNCTION__, a);
    return ctx;
}

EventContext SendContext(float a, float b)
{
    EventContext ctx; ctx.f32[0] = a; ctx.f32[1] = b;
    NOUS_TRACE("[%s] Created floatx2 context: %.3f, %.3f", __FUNCTION__, a, b);
    return ctx;
}

EventContext SendContext(float a, float b, float c)
{
    EventContext ctx; ctx.f32[0] = a; ctx.f32[1] = b; ctx.f32[2] = c;
    NOUS_TRACE("[%s] Created floatx3 context: %.3f, %.3f, %.3f", __FUNCTION__, a, b, c);
    return ctx;
}

// -------- [double] --------
EventContext SendContext(double a)
{
    EventContext ctx; ctx.f64[0] = a;
    NOUS_TRACE("[%s] Created double context: %.3f", __FUNCTION__, a);
    return ctx;
}

EventContext SendContext(double a, double b)
{
    EventContext ctx; ctx.f64[0] = a; ctx.f64[1] = b;
    NOUS_TRACE("[%s] Created doublex2 context: %.3f, %.3f", __FUNCTION__, a, b);
    return ctx;
}

// -------- [bool] --------
EventContext SendContext(bool value)
{
    EventContext ctx; ctx.u8[0] = static_cast<uint8_t>(value);
    NOUS_TRACE("[%s] Created bool context: %s", __FUNCTION__, value ? "true" : "false");
    return ctx;
}

// -------- [string] --------
EventContext SendContext(const char* str)
{
    EventContext ctx; ctx.c = str;
    NOUS_TRACE("[%s] Created string context: \"%s\"", __FUNCTION__, str ? str : "<null>");
    return ctx;
}

// -------- [pointer] --------
EventContext SendContext(void* ptr)
{
    EventContext ctx;
    ctx.u64[0] = reinterpret_cast<uintptr_t>(ptr);
    NOUS_TRACE("[%s] Created pointer context: %p", __FUNCTION__, ptr);
    return ctx;
}

// -------- [int64] --------
EventContext SendContext(int64_t a, int64_t b)
{
    EventContext ctx;
    ctx.i64[0] = a; ctx.i64[1] = b;
    NOUS_TRACE("[%s] Created int64x2 context: %lld, %lld", __FUNCTION__, (long long)a, (long long)b);
    return ctx;
}
