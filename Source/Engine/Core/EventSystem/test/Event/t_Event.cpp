#include <gtest/gtest.h>

#include <EventSystem/Event.h>
#include <EventSystem/EventSystem.h>
#include <EventSystem/IEventListener.h>

// =============================================================================
// Test listener
// =============================================================================

class MockListener : public IEventListener
{
public:
    int callCount = 0;
    Event lastEvent;

    void OnEvent(const Event& e) override
    {
        ++callCount;
        lastEvent = e;
    }
};

// =============================================================================
// Event struct
// =============================================================================

TEST(t_Event, DefaultConstructorHasTypeNone)
{
    Event e;
    EXPECT_EQ(e.type, EventType::NONE);
}

TEST(t_Event, TypeConstructorStoresType)
{
    Event e(EventType::KEY_PRESSED);
    EXPECT_EQ(e.type, EventType::KEY_PRESSED);
}

TEST(t_Event, TypeContextConstructorStoresBoth)
{
    EventContext ctx = SendContext(42);
    Event e(EventType::WINDOW_RESIZED, ctx);
    EXPECT_EQ(e.type, EventType::WINDOW_RESIZED);
    EXPECT_EQ(e.ctx.i32[0], 42);
}

// =============================================================================
// SendContext helpers
// =============================================================================

TEST(t_Event, SendContext_SingleInt32_StoresValue)
{
    EventContext ctx = SendContext(7);
    EXPECT_EQ(ctx.i32[0], 7);
}

TEST(t_Event, SendContext_TwoInt32_StoresBoth)
{
    EventContext ctx = SendContext(3, 9);
    EXPECT_EQ(ctx.i32[0], 3);
    EXPECT_EQ(ctx.i32[1], 9);
}

TEST(t_Event, SendContext_SingleFloat_StoresValue)
{
    EventContext ctx = SendContext(1.5f);
    EXPECT_FLOAT_EQ(ctx.f32[0], 1.5f);
}

TEST(t_Event, SendContext_BoolTrue_NonZero)
{
    EventContext ctx = SendContext(true);
    EXPECT_NE(ctx.u8[0], 0u);
}

TEST(t_Event, SendContext_BoolFalse_Zero)
{
    EventContext ctx = SendContext(false);
    EXPECT_EQ(ctx.u8[0], 0u);
}

TEST(t_Event, SendContext_Pointer_StoresAddress)
{
    int dummy = 42;
    EventContext ctx = SendContext(&dummy);
    EXPECT_EQ(ctx.ptr[0], static_cast<void*>(&dummy));
}

// =============================================================================
// EventSystem — Subscribe / Broadcast
// =============================================================================

class t_EventSystem : public ::testing::Test
{
protected:
    EventSystem es;
};

TEST_F(t_EventSystem, BroadcastToSingleListener_CallbackFires)
{
    MockListener listener;
    es.Subscribe(EventType::KEY_PRESSED, &listener);
    es.Broadcast(Event(EventType::KEY_PRESSED));
    EXPECT_EQ(listener.callCount, 1);
}

TEST_F(t_EventSystem, BroadcastDeliversCorrectEventType)
{
    MockListener listener;
    es.Subscribe(EventType::WINDOW_RESIZED, &listener);
    es.Broadcast(Event(EventType::WINDOW_RESIZED));
    EXPECT_EQ(listener.lastEvent.type, EventType::WINDOW_RESIZED);
}

TEST_F(t_EventSystem, BroadcastDeliversContext)
{
    MockListener listener;
    es.Subscribe(EventType::WINDOW_RESIZED, &listener);

    EventContext ctx = SendContext(1280, 720);
    es.Broadcast(Event(EventType::WINDOW_RESIZED, ctx));

    EXPECT_EQ(listener.lastEvent.ctx.i32[0], 1280);
    EXPECT_EQ(listener.lastEvent.ctx.i32[1], 720);
}

TEST_F(t_EventSystem, BroadcastToMultipleListeners_AllReceive)
{
    MockListener a, b;
    es.Subscribe(EventType::KEY_PRESSED, &a);
    es.Subscribe(EventType::KEY_PRESSED, &b);
    es.Broadcast(Event(EventType::KEY_PRESSED));
    EXPECT_EQ(a.callCount, 1);
    EXPECT_EQ(b.callCount, 1);
}

TEST_F(t_EventSystem, BroadcastDoesNotFireOtherEventTypes)
{
    MockListener listener;
    es.Subscribe(EventType::KEY_PRESSED, &listener);
    es.Broadcast(Event(EventType::WINDOW_RESIZED));
    EXPECT_EQ(listener.callCount, 0);
}

TEST_F(t_EventSystem, Unsubscribe_StopsCallback)
{
    MockListener listener;
    es.Subscribe(EventType::KEY_PRESSED, &listener);
    es.Unsubscribe(EventType::KEY_PRESSED, &listener);
    es.Broadcast(Event(EventType::KEY_PRESSED));
    EXPECT_EQ(listener.callCount, 0);
}

// =============================================================================
// EventSystem — Queue / DispatchQueued
// =============================================================================

TEST_F(t_EventSystem, QueuedEvent_NotFiredBeforeDispatch)
{
    MockListener listener;
    es.Subscribe(EventType::KEY_PRESSED, &listener);
    es.Queue(Event(EventType::KEY_PRESSED));
    EXPECT_EQ(listener.callCount, 0);
}

TEST_F(t_EventSystem, QueuedEvent_FiredAfterDispatch)
{
    MockListener listener;
    es.Subscribe(EventType::KEY_PRESSED, &listener);
    es.Queue(Event(EventType::KEY_PRESSED));
    es.DispatchQueued();
    EXPECT_EQ(listener.callCount, 1);
}

TEST_F(t_EventSystem, MultipleQueuedEvents_AllFiredAfterDispatch)
{
    MockListener listener;
    es.Subscribe(EventType::KEY_PRESSED, &listener);
    es.Queue(Event(EventType::KEY_PRESSED));
    es.Queue(Event(EventType::KEY_PRESSED));
    es.Queue(Event(EventType::KEY_PRESSED));
    es.DispatchQueued();
    EXPECT_EQ(listener.callCount, 3);
}

TEST_F(t_EventSystem, QueuedEvents_ClearedAfterDispatch)
{
    MockListener listener;
    es.Subscribe(EventType::KEY_PRESSED, &listener);
    es.Queue(Event(EventType::KEY_PRESSED));
    es.DispatchQueued();
    es.DispatchQueued();
    EXPECT_EQ(listener.callCount, 1);
}

TEST_F(t_EventSystem, Clear_RemovesAllListeners)
{
    MockListener listener;
    es.Subscribe(EventType::KEY_PRESSED, &listener);
    es.Clear();
    es.Broadcast(Event(EventType::KEY_PRESSED));
    EXPECT_EQ(listener.callCount, 0);
}
