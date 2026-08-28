// Covers the EventSystem dispatcher itself. t_EventSystem_Event next door covers
// the Event/EventContext value types; nothing exercised Subscribe/Broadcast/Queue
// until now.
//
// NOT covered here, deliberately: re-entrancy. Broadcast() holds m_Mutex across
// listener->OnEvent(), and m_Mutex is a non-recursive std::mutex, so a listener
// that calls Subscribe/Unsubscribe/Queue/Broadcast from inside its own OnEvent
// self-deadlocks. A test for that would hang the suite rather than fail it, so it
// is reported in the review notes instead of pinned here. Every fake below is
// therefore careful not to re-enter.

#include <gtest/gtest.h>

#include <EventSystem/EventSystem.h>
#include <EventSystem/Event.h>
#include <EventSystem/IEventListener.h>

#include <vector>

namespace
{
    // Records what it received, in arrival order.
    class RecordingListener final : public IEventListener
    {
    public:
        void OnEvent(const Event& evt) override
        {
            received.push_back(evt.type);
            ++callCount;
        }

        std::vector<EventType> received;
        int                    callCount = 0;
    };
}

class t_EventSystem : public ::testing::Test
{
protected:
    EventSystem bus;
    RecordingListener a;
    RecordingListener b;
};

// ---------------------------------------------------------------------------
// Subscribe / Broadcast
// ---------------------------------------------------------------------------

TEST_F(t_EventSystem, BroadcastWithNoListenersIsNoOp)
{
    EXPECT_NO_FATAL_FAILURE(bus.Broadcast(Event(EventType::KEY_PRESSED)));
}

TEST_F(t_EventSystem, SubscribedListenerReceivesItsEventType)
{
    bus.Subscribe(EventType::KEY_PRESSED, &a);

    bus.Broadcast(Event(EventType::KEY_PRESSED));

    ASSERT_EQ(a.received.size(), 1u);
    EXPECT_EQ(a.received[0], EventType::KEY_PRESSED);
}

TEST_F(t_EventSystem, ListenerDoesNotReceiveOtherEventTypes)
{
    bus.Subscribe(EventType::KEY_PRESSED, &a);

    bus.Broadcast(Event(EventType::WINDOW_RESIZED));

    EXPECT_EQ(a.callCount, 0);
}

TEST_F(t_EventSystem, MultipleListenersAllReceive_InSubscriptionOrder)
{
    // The dispatch order is the order of subscription: Subscribe push_backs and
    // Broadcast walks the vector forward. Pinned because listeners that mutate
    // shared state (module PreUpdate ordering) would be sensitive to a change.
    std::vector<int> order;
    struct OrderingListener final : IEventListener
    {
        OrderingListener(std::vector<int>& out, int id) : out(out), id(id) {}
        void OnEvent(const Event&) override { out.push_back(id); }
        std::vector<int>& out;
        int id;
    };
    OrderingListener first(order, 1);
    OrderingListener second(order, 2);

    bus.Subscribe(EventType::KEY_PRESSED, &first);
    bus.Subscribe(EventType::KEY_PRESSED, &second);
    bus.Broadcast(Event(EventType::KEY_PRESSED));

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
}

TEST_F(t_EventSystem, SameListenerSubscribedTwiceIsCalledOnce)
{
    // Subscribe dedups; without it a module that subscribes in both Awake and
    // Start would silently handle every event twice.
    bus.Subscribe(EventType::KEY_PRESSED, &a);
    bus.Subscribe(EventType::KEY_PRESSED, &a);

    bus.Broadcast(Event(EventType::KEY_PRESSED));

    EXPECT_EQ(a.callCount, 1);
}

TEST_F(t_EventSystem, OneListenerMaySubscribeToSeveralTypes)
{
    bus.Subscribe(EventType::KEY_PRESSED, &a);
    bus.Subscribe(EventType::WINDOW_RESIZED, &a);

    bus.Broadcast(Event(EventType::KEY_PRESSED));
    bus.Broadcast(Event(EventType::WINDOW_RESIZED));

    ASSERT_EQ(a.received.size(), 2u);
    EXPECT_EQ(a.received[0], EventType::KEY_PRESSED);
    EXPECT_EQ(a.received[1], EventType::WINDOW_RESIZED);
}

TEST_F(t_EventSystem, NullListenerIsAcceptedAndSkippedOnBroadcast)
{
    // Subscribe does not reject null; Broadcast guards instead. Pinning the pair
    // so neither half is "cleaned up" without the other.
    bus.Subscribe(EventType::KEY_PRESSED, nullptr);
    bus.Subscribe(EventType::KEY_PRESSED, &a);

    EXPECT_NO_FATAL_FAILURE(bus.Broadcast(Event(EventType::KEY_PRESSED)));
    EXPECT_EQ(a.callCount, 1);
}

// ---------------------------------------------------------------------------
// Unsubscribe
// ---------------------------------------------------------------------------

TEST_F(t_EventSystem, UnsubscribedListenerStopsReceiving)
{
    bus.Subscribe(EventType::KEY_PRESSED, &a);
    bus.Unsubscribe(EventType::KEY_PRESSED, &a);

    bus.Broadcast(Event(EventType::KEY_PRESSED));

    EXPECT_EQ(a.callCount, 0);
}

TEST_F(t_EventSystem, UnsubscribeLeavesOtherListenersSubscribed)
{
    bus.Subscribe(EventType::KEY_PRESSED, &a);
    bus.Subscribe(EventType::KEY_PRESSED, &b);

    bus.Unsubscribe(EventType::KEY_PRESSED, &a);
    bus.Broadcast(Event(EventType::KEY_PRESSED));

    EXPECT_EQ(a.callCount, 0);
    EXPECT_EQ(b.callCount, 1);
}

TEST_F(t_EventSystem, UnsubscribeOnlyAffectsTheNamedEventType)
{
    bus.Subscribe(EventType::KEY_PRESSED, &a);
    bus.Subscribe(EventType::WINDOW_RESIZED, &a);

    bus.Unsubscribe(EventType::KEY_PRESSED, &a);
    bus.Broadcast(Event(EventType::KEY_PRESSED));
    bus.Broadcast(Event(EventType::WINDOW_RESIZED));

    ASSERT_EQ(a.received.size(), 1u);
    EXPECT_EQ(a.received[0], EventType::WINDOW_RESIZED);
}

TEST_F(t_EventSystem, UnsubscribeFromUnregisteredTypeIsNoOp)
{
    EXPECT_NO_FATAL_FAILURE(bus.Unsubscribe(EventType::WINDOW_MINIMIZED, &a));
}

TEST_F(t_EventSystem, UnsubscribeListenerThatWasNeverSubscribedIsNoOp)
{
    bus.Subscribe(EventType::KEY_PRESSED, &a);

    EXPECT_NO_FATAL_FAILURE(bus.Unsubscribe(EventType::KEY_PRESSED, &b));
    bus.Broadcast(Event(EventType::KEY_PRESSED));
    EXPECT_EQ(a.callCount, 1);
}

TEST_F(t_EventSystem, UnsubscribeTwiceIsNoOp)
{
    bus.Subscribe(EventType::KEY_PRESSED, &a);
    bus.Unsubscribe(EventType::KEY_PRESSED, &a);

    EXPECT_NO_FATAL_FAILURE(bus.Unsubscribe(EventType::KEY_PRESSED, &a));
    EXPECT_EQ(a.callCount, 0);
}

TEST_F(t_EventSystem, ResubscribeAfterUnsubscribeWorks)
{
    bus.Subscribe(EventType::KEY_PRESSED, &a);
    bus.Unsubscribe(EventType::KEY_PRESSED, &a);
    bus.Subscribe(EventType::KEY_PRESSED, &a);

    bus.Broadcast(Event(EventType::KEY_PRESSED));

    EXPECT_EQ(a.callCount, 1);
}

// ---------------------------------------------------------------------------
// Queue / DispatchQueued
// ---------------------------------------------------------------------------

TEST_F(t_EventSystem, QueuedEventIsNotDeliveredUntilDispatch)
{
    bus.Subscribe(EventType::KEY_PRESSED, &a);

    bus.Queue(Event(EventType::KEY_PRESSED));
    EXPECT_EQ(a.callCount, 0);

    bus.DispatchQueued();
    EXPECT_EQ(a.callCount, 1);
}

TEST_F(t_EventSystem, DispatchDrainsTheQueue)
{
    bus.Subscribe(EventType::KEY_PRESSED, &a);
    bus.Queue(Event(EventType::KEY_PRESSED));

    bus.DispatchQueued();
    bus.DispatchQueued();   // nothing left to deliver

    EXPECT_EQ(a.callCount, 1);
}

TEST_F(t_EventSystem, DispatchWithEmptyQueueIsNoOp)
{
    bus.Subscribe(EventType::KEY_PRESSED, &a);

    EXPECT_NO_FATAL_FAILURE(bus.DispatchQueued());
    EXPECT_EQ(a.callCount, 0);
}

TEST_F(t_EventSystem, QueuedEventsDispatchInFIFOOrder)
{
    bus.Subscribe(EventType::KEY_PRESSED, &a);
    bus.Subscribe(EventType::WINDOW_RESIZED, &a);

    bus.Queue(Event(EventType::WINDOW_RESIZED));
    bus.Queue(Event(EventType::KEY_PRESSED));
    bus.DispatchQueued();

    ASSERT_EQ(a.received.size(), 2u);
    EXPECT_EQ(a.received[0], EventType::WINDOW_RESIZED);
    EXPECT_EQ(a.received[1], EventType::KEY_PRESSED);
}

TEST_F(t_EventSystem, QueuedEventForAnUnsubscribedTypeIsDroppedSilently)
{
    bus.Queue(Event(EventType::KEY_PRESSED));

    EXPECT_NO_FATAL_FAILURE(bus.DispatchQueued());
    EXPECT_EQ(a.callCount, 0);
}

TEST_F(t_EventSystem, ListenerUnsubscribedBeforeDispatchDoesNotReceiveQueuedEvent)
{
    // The queue stores events, not resolved listener lists, so unsubscribing
    // between Queue and DispatchQueued takes effect. This is what makes it safe
    // for a module to unsubscribe in CleanUp with work still queued.
    bus.Subscribe(EventType::KEY_PRESSED, &a);
    bus.Queue(Event(EventType::KEY_PRESSED));
    bus.Unsubscribe(EventType::KEY_PRESSED, &a);

    bus.DispatchQueued();

    EXPECT_EQ(a.callCount, 0);
}

TEST_F(t_EventSystem, QueuedEventCarriesItsContext)
{
    struct ContextListener final : IEventListener
    {
        void OnEvent(const Event& evt) override { seen = evt.ctx.i32[0]; }
        int32_t seen = 0;
    } listener;

    bus.Subscribe(EventType::WINDOW_RESIZED, &listener);

    Event evt(EventType::WINDOW_RESIZED);
    evt.ctx = SendContext(1920);
    bus.Queue(evt);
    bus.DispatchQueued();

    EXPECT_EQ(listener.seen, 1920);
}

// ---------------------------------------------------------------------------
// Clear
// ---------------------------------------------------------------------------

TEST_F(t_EventSystem, ClearRemovesAllListeners)
{
    bus.Subscribe(EventType::KEY_PRESSED, &a);
    bus.Subscribe(EventType::WINDOW_RESIZED, &b);

    bus.Clear();
    bus.Broadcast(Event(EventType::KEY_PRESSED));
    bus.Broadcast(Event(EventType::WINDOW_RESIZED));

    EXPECT_EQ(a.callCount, 0);
    EXPECT_EQ(b.callCount, 0);
}

TEST_F(t_EventSystem, ClearDropsQueuedEvents)
{
    bus.Subscribe(EventType::KEY_PRESSED, &a);
    bus.Queue(Event(EventType::KEY_PRESSED));

    bus.Clear();
    bus.Subscribe(EventType::KEY_PRESSED, &a);   // re-subscribe after the wipe
    bus.DispatchQueued();

    EXPECT_EQ(a.callCount, 0);
}

TEST_F(t_EventSystem, SubscribeWorksAgainAfterClear)
{
    bus.Subscribe(EventType::KEY_PRESSED, &a);
    bus.Clear();

    bus.Subscribe(EventType::KEY_PRESSED, &a);
    bus.Broadcast(Event(EventType::KEY_PRESSED));

    EXPECT_EQ(a.callCount, 1);
}
