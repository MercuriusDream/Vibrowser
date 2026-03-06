#include <clever/platform/timer.h>

#include <gtest/gtest.h>

#include <chrono>
#include <string>
#include <vector>

using clever::platform::TimerScheduler;
using namespace std::chrono_literals;

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
