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
            should_quit = tick_observations.size() == 2;
        }
        if (should_quit) {
            loop.quit();
        }
    });

    loop.post_task([&]() {
        std::this_thread::sleep_for(185ms);
    });

    runner.join();

    ASSERT_EQ(tick_observations.size(), 2u);
    EXPECT_GE(tick_observations[0].elapsed, 170ms);
    EXPECT_GE(tick_observations[0].lag, 100ms);
    EXPECT_LT(tick_observations[1].lag, 40ms);
    EXPECT_LT(tick_observations[1].lag, tick_observations[0].lag);
    EXPECT_TRUE(timer->is_active());
    timer->cancel();
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
