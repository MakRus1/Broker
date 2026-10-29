#include <optional>
#include <platform/outbox/worker.hpp>
#include <userver/engine/single_consumer_event.hpp>
#include <userver/engine/sleep.hpp>
#include <userver/utest/utest.hpp>

namespace {
struct Message {
    int Lease = 7;
};
}  // namespace

UTEST(Outbox, EmptyQueueDoesNotSendOrAcknowledge) {
    auto claim = [] { return std::optional<Message>{}; };
    auto send = [](Message&) { ADD_FAILURE(); };
    auto complete = [](Message&) { ADD_FAILURE(); };
    auto retry = [](Message&) { ADD_FAILURE(); };
    EXPECT_FALSE(platform::outbox::DeliverOne(claim, send, complete, retry));
}

UTEST(Outbox, DeliveryResultIsPassedToAcknowledgement) {
    auto claim = [] { return std::optional{Message{}}; };
    auto send = [](Message&) { return 42; };
    auto complete = [](Message& message, int result) {
        EXPECT_EQ(message.Lease, 7);
        EXPECT_EQ(result, 42);
    };
    auto retry = [](Message&) { ADD_FAILURE(); };
    EXPECT_TRUE(platform::outbox::DeliverOne(claim, send, complete, retry));
}

UTEST(Outbox, FailedSendRetriesTheClaimedLeaseWithoutAcknowledging) {
    int retries = 0;
    auto claim = [] { return std::optional{Message{}}; };
    auto send = [](Message&) { throw std::runtime_error{"Unavailable"}; };
    auto complete = [](Message&) { ADD_FAILURE(); };
    auto retry = [&](Message& message) {
        EXPECT_EQ(message.Lease, 7);
        ++retries;
    };
    EXPECT_TRUE(platform::outbox::DeliverOne(claim, send, complete, retry));
    EXPECT_EQ(retries, 1);
}

UTEST(Outbox, LostAcknowledgementAllowsRepeatedDelivery) {
    int sends = 0;
    int retries = 0;
    auto claim = [] { return std::optional{Message{}}; };
    auto send = [&](Message&) { ++sends; };
    auto complete = [&](Message&) {
        if (sends == 1)
            throw std::runtime_error{"Lost acknowledgement"};
    };
    auto retry = [&](Message&) { ++retries; };
    EXPECT_TRUE(platform::outbox::DeliverOne(claim, send, complete, retry));
    EXPECT_TRUE(platform::outbox::DeliverOne(claim, send, complete, retry));
    EXPECT_EQ(sends, 2);
    EXPECT_EQ(retries, 1);
}

UTEST(Outbox, BatchStopsAtLimitOrEmptyQueue) {
    int calls = 0;
    EXPECT_EQ(platform::outbox::DeliverBatch(3,
                  [&] {
                      ++calls;
                      return true;
                  }),
        3);
    EXPECT_EQ(calls, 3);
    calls = 0;
    EXPECT_EQ(platform::outbox::DeliverBatch(50, [&] { return ++calls < 3; }), 2);
    EXPECT_EQ(calls, 3);
}

UTEST(Outbox, BatchPreservesStateOfItsCallback) {
    auto step = [calls = 0]() mutable { return ++calls < 3; };
    EXPECT_EQ(platform::outbox::DeliverBatch(1, step), 1);
    EXPECT_EQ(platform::outbox::DeliverBatch(1, step), 1);
    EXPECT_EQ(platform::outbox::DeliverBatch(1, step), 0);
}

UTEST(Outbox, EmptyQueueUsesIdlePauseAndStopInterruptsSleep) {
    platform::outbox::Worker worker;
    userver::engine::SingleConsumerEvent first_poll;
    int polls = 0;
    worker.Start(
        "test-outbox-idle",
        {.BatchSize = 3,
            .BusyPause = std::chrono::milliseconds{1},
            .IdlePause = std::chrono::milliseconds{20}},
        [&]() -> std::optional<Message> {
            ++polls;
            first_poll.Send();
            return std::nullopt;
        },
        [](Message&) { ADD_FAILURE(); }, [](Message&) { ADD_FAILURE(); },
        [](Message&) { ADD_FAILURE(); });
    ASSERT_TRUE(first_poll.WaitForEventFor(std::chrono::seconds{1}));
    userver::engine::SleepFor(std::chrono::milliseconds{100});
    worker.Stop();
    EXPECT_GE(polls, 2);
    EXPECT_LE(polls, 10);
    const auto stopped_polls = polls;
    userver::engine::SleepFor(std::chrono::milliseconds{30});
    EXPECT_EQ(polls, stopped_polls);
}

UTEST(Outbox, StopDoesNotRetryCancelledDelivery) {
    platform::outbox::Worker worker;
    userver::engine::SingleConsumerEvent sending;
    int retries = 0;
    int acknowledgements = 0;
    worker.Start(
        "test-outbox-cancel",
        {.BatchSize = 3,
            .BusyPause = std::chrono::milliseconds{1},
            .IdlePause = std::chrono::milliseconds{1}},
        [] { return std::optional{Message{}}; },
        [&](Message&) {
            sending.Send();
            userver::engine::InterruptibleSleepFor(std::chrono::minutes{1});
            throw std::runtime_error{"Cancelled transport"};
        },
        [&](Message&) { ++acknowledgements; }, [&](Message&) { ++retries; });
    ASSERT_TRUE(sending.WaitForEventFor(std::chrono::seconds{1}));
    worker.Stop();
    EXPECT_EQ(retries, 0);
    EXPECT_EQ(acknowledgements, 0);
}
