#include <clever/platform/event_loop.h>
#include <clever/platform/timer.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using clever::platform::EventLoop;
using clever::platform::Timer;
using clever::platform::TimerScheduler;
using namespace std::chrono_literals;

namespace {

// The recovered tick spacing is measured from truncated millisecond samples,
// so allow 1ms of granularity slack while still rejecting timer bursts.
constexpr auto kRecoveredTickMinimumSpacing = 9ms;

struct RearmProbeState {
    std::mutex mutex;
    std::condition_variable cv;
    bool callback_started = false;
    bool release_callback = false;
};

struct RearmProbeCallback {
    std::shared_ptr<RearmProbeState> state;
    std::atomic<int>* ticks = nullptr;

    RearmProbeCallback(std::shared_ptr<RearmProbeState> state_, std::atomic<int>* ticks_)
        : state(std::move(state_))
        , ticks(ticks_) {}

    void operator()() const {
        ticks->fetch_add(1, std::memory_order_relaxed);

        std::unique_lock lock(state->mutex);
        state->callback_started = true;
        state->cv.notify_all();
        state->cv.wait(lock, [&]() { return state->release_callback; });
    }
};

} // namespace

TEST(TimerSchedulerTest, TimeoutsFireInDueTimeThenInsertionOrder) {
    TimerScheduler scheduler;
    std::vector<std::string> order;

    scheduler.schedule_timeout(10ms, [&]() { order.push_back("first"); });
    scheduler.schedule_timeout(10ms, [&]() { order.push_back("second"); });
    scheduler.schedule_timeout(25ms, [&]() { order.push_back("third"); });

    EXPECT_EQ(scheduler.run_ready(9ms), 0u);
    EXPECT_TRUE(order.empty());

    EXPECT_EQ(scheduler.run_ready(1ms), 2u);
    EXPECT_EQ(order, (std::vector<std::string> { "first", "second" }));

    EXPECT_EQ(scheduler.run_ready(14ms), 0u);
    EXPECT_EQ(order, (std::vector<std::string> { "first", "second" }));

    EXPECT_EQ(scheduler.run_ready(1ms), 1u);
    EXPECT_EQ(order, (std::vector<std::string> { "first", "second", "third" }));
}

TEST(TimerSchedulerTest, CancelBeforeFirePreventsCallback) {
    TimerScheduler scheduler;
    bool fired = false;

    int id = scheduler.schedule_timeout(5ms, [&]() { fired = true; });
    EXPECT_TRUE(scheduler.is_active(id));
    EXPECT_TRUE(scheduler.cancel(id));
    EXPECT_FALSE(scheduler.is_active(id));
    EXPECT_FALSE(scheduler.cancel(id));

    EXPECT_EQ(scheduler.run_ready(5ms), 0u);
    EXPECT_FALSE(fired);
}

TEST(TimerSchedulerTest, TimeoutBecomesInactiveOnceFireStarts) {
    TimerScheduler scheduler;
    bool active_in_callback = true;
    bool cancel_result = true;

    int id = 0;
    id = scheduler.schedule_timeout(5ms, [&]() {
        active_in_callback = scheduler.is_active(id);
        cancel_result = scheduler.cancel(id);
    });

    EXPECT_EQ(scheduler.run_ready(5ms), 1u);
    EXPECT_FALSE(active_in_callback);
    EXPECT_FALSE(cancel_result);
    EXPECT_FALSE(scheduler.is_active(id));
}

TEST(TimerSchedulerTest, EarlierReadyTimerCanCancelLaterReadyTimer) {
    TimerScheduler scheduler;
    std::vector<std::string> order;

    int victim_id = 0;
    scheduler.schedule_timeout(0ms, [&]() {
        order.push_back("canceller");
        EXPECT_TRUE(scheduler.cancel(victim_id));
    });
    victim_id = scheduler.schedule_timeout(0ms, [&]() { order.push_back("victim"); });

    EXPECT_EQ(scheduler.run_ready(), 1u);
    EXPECT_EQ(order, (std::vector<std::string> { "canceller" }));
    EXPECT_FALSE(scheduler.is_active(victim_id));
}

TEST(TimerSchedulerTest, IntervalCanCancelItselfDuringCallback) {
    TimerScheduler scheduler;
    std::vector<int> ticks;

    int interval_id = 0;
    interval_id = scheduler.schedule_interval(10ms, [&]() {
        ticks.push_back(static_cast<int>(ticks.size()) + 1);
        EXPECT_TRUE(scheduler.cancel(interval_id));
    });

    EXPECT_EQ(scheduler.run_ready(10ms), 1u);
    EXPECT_EQ(ticks, (std::vector<int> { 1 }));
    EXPECT_FALSE(scheduler.is_active(interval_id));

    EXPECT_EQ(scheduler.run_ready(50ms), 0u);
    EXPECT_EQ(ticks, (std::vector<int> { 1 }));
}

TEST(TimerSchedulerTest, IntervalCatchUpKeepsOriginalCadenceAfterCoarseAdvance) {
    TimerScheduler scheduler;
    std::vector<int> ticks;

    int id = scheduler.schedule_interval(10ms, [&]() {
        ticks.push_back(static_cast<int>(ticks.size()) + 1);
    });

    EXPECT_EQ(scheduler.run_ready(35ms), 1u);
    EXPECT_EQ(ticks, (std::vector<int> { 1 }));
    EXPECT_TRUE(scheduler.is_active(id));

    EXPECT_EQ(scheduler.run_ready(4ms), 0u);
    EXPECT_EQ(ticks, (std::vector<int> { 1 }));

    EXPECT_EQ(scheduler.run_ready(1ms), 1u);
    EXPECT_EQ(ticks, (std::vector<int> { 1, 2 }));

    EXPECT_EQ(scheduler.run_ready(9ms), 0u);
    EXPECT_EQ(scheduler.run_ready(1ms), 1u);
    EXPECT_EQ(ticks, (std::vector<int> { 1, 2, 3 }));
}

TEST(TimerSchedulerTest, IntervalCatchUpClampsVeryLongAdvanceWithoutBurst) {
    TimerScheduler scheduler;
    std::vector<int> ticks;

    int id = scheduler.schedule_interval(10ms, [&]() {
        ticks.push_back(static_cast<int>(ticks.size()) + 1);
    });

    EXPECT_EQ(scheduler.run_ready(10s), 1u);
    EXPECT_EQ(ticks, (std::vector<int> { 1 }));
    EXPECT_TRUE(scheduler.is_active(id));

    EXPECT_EQ(scheduler.run_ready(9ms), 0u);
    EXPECT_EQ(ticks, (std::vector<int> { 1 }));

    EXPECT_EQ(scheduler.run_ready(1ms), 1u);
    EXPECT_EQ(ticks, (std::vector<int> { 1, 2 }));
}

TEST(TimerTest, RepeatingEventLoopTimerSkipsMissedTicksV2065) {
    EventLoop loop;
    struct TickObservation {
        std::chrono::milliseconds elapsed { 0 };
        std::chrono::milliseconds lag { 0 };
    };

    std::vector<TickObservation> tick_observations;
    std::mutex tick_mutex;

    std::thread runner([&]() { loop.run(); });

    auto wait_started_until = std::chrono::steady_clock::now() + 500ms;
    while (!loop.is_running() && std::chrono::steady_clock::now() < wait_started_until) {
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_TRUE(loop.is_running());

    auto start = std::chrono::steady_clock::now();

    auto timer = Timer::repeating(loop, 50ms, [&]() {
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        auto lag = std::chrono::duration_cast<std::chrono::milliseconds>(loop.last_delayed_task_lag());
        bool should_quit = false;
        {
            std::lock_guard lock(tick_mutex);
            tick_observations.push_back(TickObservation { elapsed, lag });
            should_quit = tick_observations.size() == 3;
        }
        if (should_quit) {
            loop.quit();
        }
    });

    loop.post_task([&]() {
        std::this_thread::sleep_for(165ms);
    });

    runner.join();

    ASSERT_EQ(tick_observations.size(), 3u);
    EXPECT_GE(tick_observations[0].elapsed, 150ms);
    EXPECT_GE(tick_observations[0].lag, 100ms);
    EXPECT_GE(tick_observations[1].elapsed - tick_observations[0].elapsed, kRecoveredTickMinimumSpacing);
    EXPECT_LE(tick_observations[1].elapsed - tick_observations[0].elapsed, 80ms);
    EXPECT_GE(tick_observations[2].elapsed - tick_observations[1].elapsed, 40ms);
    EXPECT_LE(tick_observations[2].elapsed - tick_observations[1].elapsed, 70ms);
    EXPECT_LT(tick_observations[1].lag, tick_observations[0].lag);
    EXPECT_TRUE(timer->is_active());
    timer->cancel();
}

TEST(TimerTest, RepeatingEventLoopTimerRecoversCadenceAfterLongCallbackOccupancyV2107) {
    EventLoop loop;
    std::mutex tick_mutex;
    std::vector<std::chrono::milliseconds> tick_offsets;

    std::thread runner([&]() { loop.run(); });

    auto wait_started_until = std::chrono::steady_clock::now() + 500ms;
    while (!loop.is_running() && std::chrono::steady_clock::now() < wait_started_until) {
        std::this_thread::sleep_for(1ms);
    }
    ASSERT_TRUE(loop.is_running());

    auto start = std::chrono::steady_clock::now();

    auto timer = Timer::repeating(loop, 40ms, [&]() {
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);

        bool should_quit = false;
        bool should_stall = false;
        {
            std::lock_guard lock(tick_mutex);
            tick_offsets.push_back(elapsed);
            should_stall = tick_offsets.size() == 1;
            should_quit = tick_offsets.size() == 3;
        }

        if (should_stall) {
            std::this_thread::sleep_for(95ms);
        }
        if (should_quit) {
            loop.quit();
        }
    });

    runner.join();

    ASSERT_EQ(tick_offsets.size(), 3u);
    EXPECT_GE(tick_offsets[0], 35ms);
    EXPECT_GE(tick_offsets[1] - tick_offsets[0], 110ms);
    EXPECT_LE(tick_offsets[1] - tick_offsets[0], 145ms);
    EXPECT_GE(tick_offsets[2] - tick_offsets[1], 30ms);
    EXPECT_LE(tick_offsets[2] - tick_offsets[1], 70ms);
    EXPECT_TRUE(timer->is_active());
    timer->cancel();
}

TEST(TimerTest, OneShotEventLoopTimerBecomesInactiveAfterFireV2069) {
    EventLoop loop;
    std::mutex mutex;
    std::condition_variable cv;
    bool fired = false;
    bool active_after_callback = true;
    std::unique_ptr<Timer> timer;

    timer = Timer::one_shot(loop, 20ms, [&]() {
        {
            std::lock_guard lock(mutex);
            fired = true;
            active_after_callback = timer->is_active();
        }
        cv.notify_all();
        loop.quit();
    });

    std::thread runner([&]() { loop.run(); });

    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, 500ms, [&]() { return fired; }));
    }

    runner.join();

    EXPECT_FALSE(active_after_callback);
    EXPECT_FALSE(timer->is_active());
}

TEST(TimerTest, ZeroDelayOneShotPreservesTimerSourcePriorityAgainstQueuedWork) {
    EventLoop loop;
    std::vector<std::string> order;

    auto timer = Timer::one_shot(loop, 0ms, [&]() { order.push_back("timer"); });
    std::this_thread::sleep_for(2ms);

    loop.post_task([&order]() { order.push_back("render"); }, EventLoop::TaskSource::kRender);
    loop.post_task([&order]() { order.push_back("idle"); }, EventLoop::TaskSource::kIdle);
    loop.post_task([&order]() { order.push_back("network"); }, EventLoop::TaskSource::kNetwork);
    loop.post_task([&order]() { order.push_back("input"); }, EventLoop::TaskSource::kInput);

    loop.run_pending();

    ASSERT_EQ(order.size(), 5u);
    EXPECT_EQ(order[0], "input");
    EXPECT_EQ(order[1], "network");
    EXPECT_EQ(order[2], "timer");
    EXPECT_EQ(order[3], "render");
    EXPECT_EQ(order[4], "idle");
    EXPECT_FALSE(timer->is_active());
}

TEST(TimerTest, CancelledOneShotEventLoopTimerStaysInactiveWithoutCallbackV2069) {
    EventLoop loop;
    std::atomic<int> callbacks { 0 };

    auto timer = Timer::one_shot(loop, 40ms, [&]() {
        callbacks.fetch_add(1, std::memory_order_relaxed);
    });

    std::thread runner([&]() { loop.run(); });

    timer->cancel();
    EXPECT_FALSE(timer->is_active());

    std::this_thread::sleep_for(80ms);
    EXPECT_EQ(callbacks.load(std::memory_order_relaxed), 0);
    EXPECT_FALSE(timer->is_active());

    loop.quit();
    runner.join();
}

TEST(TimerTest, CancelledRepeatingEventLoopTimerDoesNotRearmV2065) {
    EventLoop loop;
    auto probe_state = std::make_shared<RearmProbeState>();
    std::atomic<int> ticks { 0 };

    auto timer = Timer::repeating(loop, 40ms, RearmProbeCallback { probe_state, &ticks });
    std::thread runner([&]() { loop.run(); });

    {
        std::unique_lock lock(probe_state->mutex);
        ASSERT_TRUE(probe_state->cv.wait_for(lock, 500ms, [&]() { return probe_state->callback_started; }));
    }

    timer->cancel();
    {
        std::lock_guard lock(probe_state->mutex);
        probe_state->release_callback = true;
    }
    probe_state->cv.notify_all();

    std::this_thread::sleep_for(15ms);
    EXPECT_EQ(loop.pending_count(), 0u);
    EXPECT_EQ(ticks.load(std::memory_order_relaxed), 1);
    EXPECT_FALSE(timer->is_active());

    loop.quit();
    runner.join();
}

TEST(TimerTest, SelfCancelledRepeatingEventLoopTimerDoesNotRearmAfterCoarseWakeV2105) {
    EventLoop loop;
    std::mutex mutex;
    std::condition_variable cv;
    bool cancelled = false;
    bool active_before_cancel = false;
    bool active_after_cancel = true;
    std::atomic<int> ticks { 0 };
    std::unique_ptr<Timer> timer;

    timer = Timer::repeating(loop, 40ms, [&]() {
        ticks.fetch_add(1, std::memory_order_relaxed);
        active_before_cancel = timer->is_active();
        timer->cancel();
        active_after_cancel = timer->is_active();

        {
            std::lock_guard lock(mutex);
            cancelled = true;
        }
        cv.notify_all();
    });

    std::thread runner([&]() { loop.run(); });

    loop.post_task([&]() {
        std::this_thread::sleep_for(125ms);
    });

    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, 500ms, [&]() { return cancelled; }));
    }

    std::this_thread::sleep_for(20ms);
    EXPECT_TRUE(active_before_cancel);
    EXPECT_FALSE(active_after_cancel);
    EXPECT_FALSE(timer->is_active());
    EXPECT_EQ(ticks.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(loop.pending_count(), 0u);

    loop.quit();
    runner.join();
}

TEST(TimerTest, ReentrantSiblingCancelPreventsDuplicateRepeatingRearmV2107) {
    EventLoop loop;
    std::mutex mutex;
    std::condition_variable cv;
    bool sibling_cancel_drained = false;
    std::atomic<int> primary_ticks { 0 };
    std::atomic<int> sibling_ticks { 0 };
    std::unique_ptr<Timer> primary_timer;
    std::unique_ptr<Timer> sibling_timer;

    primary_timer = Timer::repeating(loop, 30ms, [&]() {
        const auto tick = primary_ticks.fetch_add(1, std::memory_order_relaxed) + 1;
        if (tick == 1) {
            sibling_timer->cancel();
            loop.run_pending();
            {
                std::lock_guard lock(mutex);
                sibling_cancel_drained = true;
            }
            cv.notify_all();
            return;
        }
        if (tick == 3) {
            loop.quit();
        }
    });

    sibling_timer = Timer::repeating(loop, 30ms, [&]() {
        sibling_ticks.fetch_add(1, std::memory_order_relaxed);
    });

    std::thread runner([&]() { loop.run(); });

    {
        std::unique_lock lock(mutex);
        ASSERT_TRUE(cv.wait_for(lock, 500ms, [&]() { return sibling_cancel_drained; }));
    }

    auto wait_for_primary_rearm_until = std::chrono::steady_clock::now() + 100ms;
    while (std::chrono::steady_clock::now() < wait_for_primary_rearm_until
        && loop.pending_count() == 0) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_EQ(loop.pending_count(), 1u);

    runner.join();

    EXPECT_EQ(primary_ticks.load(std::memory_order_relaxed), 3);
    EXPECT_EQ(sibling_ticks.load(std::memory_order_relaxed), 0);
    EXPECT_FALSE(sibling_timer->is_active());
    EXPECT_TRUE(primary_timer->is_active());

    primary_timer->cancel();
}
