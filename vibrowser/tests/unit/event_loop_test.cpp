#include <clever/platform/event_loop.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace clever::platform;
using namespace std::chrono_literals;

namespace {

template <typename Predicate>
bool wait_until_true(Predicate&& predicate, std::chrono::milliseconds timeout) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (std::chrono::steady_clock::now() < deadline) {
        if (predicate()) {
            return true;
        }
        std::this_thread::sleep_for(1ms);
    }
    return predicate();
}

} // namespace

// ---------------------------------------------------------------------------
// 1. Post task and run_pending executes it
// ---------------------------------------------------------------------------
TEST(EventLoopTest, PostTaskAndRunPendingExecutesIt) {
    EventLoop loop;
    bool executed = false;

    loop.post_task([&executed]() { executed = true; });
    loop.run_pending();

    EXPECT_TRUE(executed);
}

// ---------------------------------------------------------------------------
// 2. Post multiple tasks — all execute in order
// ---------------------------------------------------------------------------
TEST(EventLoopTest, PostMultipleTasksAllExecuteInOrder) {
    EventLoop loop;
    std::vector<int> order;

    for (int i = 0; i < 10; ++i) {
        loop.post_task([i, &order]() { order.push_back(i); });
    }

    loop.run_pending();

    ASSERT_EQ(order.size(), 10u);
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(order[i], i);
    }
}

// ---------------------------------------------------------------------------
// 3. Post delayed task — executes after delay
// ---------------------------------------------------------------------------
TEST(EventLoopTest, PostDelayedTaskExecutesAfterDelay) {
    EventLoop loop;
    bool executed = false;

    loop.post_delayed_task([&executed]() { executed = true; }, 50ms);

    // Should not execute immediately
    loop.run_pending();
    EXPECT_FALSE(executed);

    // Wait for the delay to elapse
    std::this_thread::sleep_for(100ms);
    loop.run_pending();
    EXPECT_TRUE(executed);
}

// ---------------------------------------------------------------------------
// 4. Delayed task does not execute before its time
// ---------------------------------------------------------------------------
TEST(EventLoopTest, DelayedTaskDoesNotExecuteBeforeItsTime) {
    EventLoop loop;
    bool executed = false;

    loop.post_delayed_task([&executed]() { executed = true; }, 200ms);

    // Run pending immediately — should not fire
    loop.run_pending();
    EXPECT_FALSE(executed);

    // Wait only part of the delay
    std::this_thread::sleep_for(50ms);
    loop.run_pending();
    EXPECT_FALSE(executed);
}

// ---------------------------------------------------------------------------
// 5. quit() stops run()
// ---------------------------------------------------------------------------
TEST(EventLoopTest, QuitStopsRun) {
    EventLoop loop;

    // Post a task that calls quit after a short delay
    loop.post_task([&loop]() {
        loop.quit();
    });

    // run() should return once quit() is called
    loop.run();

    EXPECT_FALSE(loop.is_running());
}

// ---------------------------------------------------------------------------
// 6. run_pending with no tasks returns immediately
// ---------------------------------------------------------------------------
TEST(EventLoopTest, RunPendingWithNoTasksReturnsImmediately) {
    EventLoop loop;

    auto start = std::chrono::steady_clock::now();
    loop.run_pending();
    auto elapsed = std::chrono::steady_clock::now() - start;

    // Should return almost instantly (well under 100ms)
    EXPECT_LT(elapsed, 100ms);
}

// ---------------------------------------------------------------------------
// 7. pending_count reports correctly
// ---------------------------------------------------------------------------
TEST(EventLoopTest, PendingCountReportsCorrectly) {
    EventLoop loop;
    EXPECT_EQ(loop.pending_count(), 0u);

    loop.post_task([]() {});
    loop.post_task([]() {});
    loop.post_task([]() {});
    EXPECT_EQ(loop.pending_count(), 3u);

    loop.run_pending();
    EXPECT_EQ(loop.pending_count(), 0u);
}

// ---------------------------------------------------------------------------
// 8. run_pending mirrors run() and drains nested posts immediately
// ---------------------------------------------------------------------------
TEST(EventLoopTest, RunPendingDrainsPostTaskFromWithinATask) {
    EventLoop loop;
    bool inner_executed = false;

    loop.post_task([&loop, &inner_executed]() {
        loop.post_task([&inner_executed]() {
            inner_executed = true;
        });
    });

    // run_pending keeps draining until the queue is empty, so nested posts run
    // in the same call just like they do under run().
    loop.run_pending();

    EXPECT_TRUE(inner_executed);
    EXPECT_EQ(loop.pending_count(), 0u);
}

// ---------------------------------------------------------------------------
// 9. Multiple delayed tasks execute in correct order
// ---------------------------------------------------------------------------
TEST(EventLoopTest, MultipleDelayedTasksExecuteInCorrectOrder) {
    EventLoop loop;
    std::vector<int> order;

    // Post delayed tasks in reverse delay order
    loop.post_delayed_task([&order]() { order.push_back(3); }, 150ms);
    loop.post_delayed_task([&order]() { order.push_back(1); }, 50ms);
    loop.post_delayed_task([&order]() { order.push_back(2); }, 100ms);

    // Wait for all delays to elapse
    std::this_thread::sleep_for(250ms);
    loop.run_pending();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

// ---------------------------------------------------------------------------
// 10. Post task wakes up run() from blocking
// ---------------------------------------------------------------------------
TEST(EventLoopTest, PostTaskWakesUpRunFromBlocking) {
    EventLoop loop;
    std::atomic<bool> task_executed{false};

    // Start run() on a background thread
    std::thread runner([&loop]() {
        loop.run();
    });

    ASSERT_TRUE(wait_until_true([&loop]() { return loop.is_running(); }, 200ms));

    // Post a task that sets the flag and then quits
    loop.post_task([&task_executed, &loop]() {
        task_executed.store(true);
        loop.quit();
    });

    runner.join();

    EXPECT_TRUE(task_executed.load());
    EXPECT_FALSE(loop.is_running());
}

TEST(EventLoopTest, EarlierDelayedTaskWakesRun) {
    EventLoop loop;
    std::condition_variable fired_cv;
    std::mutex fired_mutex;
    bool fired = false;
    auto fired_at = EventLoop::Clock::time_point{};
    constexpr auto kOriginalDelay = 3s;
    constexpr auto kInsertedDelay = 30ms;

    loop.post_delayed_task([]() {}, kOriginalDelay);
    std::thread runner([&loop]() {
        loop.run();
    });

    ASSERT_TRUE(wait_until_true([&loop]() { return loop.is_running(); }, 200ms));
    ASSERT_TRUE(wait_until_true([&loop]() { return loop.is_waiting_on_delayed_task(); }, 200ms));
    const auto wake_count_before = loop.earlier_deadline_wake_count();

    const auto inserted_at = EventLoop::Clock::now();
    loop.post_delayed_task([&]() {
        {
            std::lock_guard lock(fired_mutex);
            fired = true;
            fired_at = EventLoop::Clock::now();
        }
        fired_cv.notify_one();
        loop.quit();
    }, kInsertedDelay);

    std::unique_lock lock(fired_mutex);
    const bool fired_in_time = fired_cv.wait_for(lock, 400ms, [&]() { return fired; });
    lock.unlock();
    if (!fired_in_time) {
        loop.quit();
    }
    runner.join();

    ASSERT_TRUE(fired_in_time);
    EXPECT_EQ(loop.earlier_deadline_wake_count(), wake_count_before + 1);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(fired_at - inserted_at);
    EXPECT_GE(elapsed, 20ms);
    EXPECT_LT(elapsed, 400ms);
}

TEST(EventLoopTest, EarlierDelayedTaskBecomesRunnableWithoutWaitingForOlderDeadline) {
    EventLoop loop;
    std::vector<int> order;

    constexpr auto kOlderDelay = 1000ms;
    constexpr auto kInsertedDelay = 20ms;

    loop.post_delayed_task([&order]() { order.push_back(2); }, kOlderDelay);

    std::this_thread::sleep_for(50ms);
    loop.post_delayed_task([&order]() { order.push_back(1); }, kInsertedDelay);

    std::this_thread::sleep_for(80ms);
    loop.run_pending();

    ASSERT_EQ(order.size(), 1u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(loop.pending_count(), 1u);

    std::this_thread::sleep_for(kOlderDelay);
    loop.run_pending();

    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(loop.pending_count(), 0u);
}

TEST(EventLoopTest, EarlierDelayedWakePreservesTaskSourcePriorityAfterResume) {
    EventLoop loop;
    std::vector<std::string> order;
    constexpr auto kOriginalDelay = 3s;
    constexpr auto kInsertedDelay = 30ms;

    loop.post_delayed_task([]() {}, kOriginalDelay);
    std::thread runner([&loop]() {
        loop.run();
    });

    ASSERT_TRUE(wait_until_true([&loop]() { return loop.is_running(); }, 200ms));
    ASSERT_TRUE(wait_until_true([&loop]() { return loop.is_waiting_on_delayed_task(); }, 200ms));
    const auto wake_count_before = loop.earlier_deadline_wake_count();

    loop.post_delayed_task([&]() {
        order.push_back("timer");
        loop.post_task([&order]() { order.push_back("network"); }, EventLoop::TaskSource::kNetwork);
        loop.post_task([&order]() { order.push_back("input"); }, EventLoop::TaskSource::kInput);
        loop.post_task([&order, &loop]() {
            order.push_back("idle");
            loop.quit();
        }, EventLoop::TaskSource::kIdle);
    }, kInsertedDelay);

    runner.join();

    EXPECT_EQ(loop.earlier_deadline_wake_count(), wake_count_before + 1);
    EXPECT_EQ(order, (std::vector<std::string>{
        "timer",
        "input",
        "network",
        "idle",
    }));
}

TEST(EventLoopTest, EarlierDelayedWakePromotesZeroDelayTimerIntoFullSourceOrder) {
    EventLoop loop;
    std::vector<std::string> order;
    constexpr auto kOriginalDelay = 3s;

    loop.post_delayed_task([]() {}, kOriginalDelay);
    std::thread runner([&loop]() {
        loop.run();
    });

    ASSERT_TRUE(wait_until_true([&loop]() { return loop.is_running(); }, 200ms));
    ASSERT_TRUE(wait_until_true([&loop]() { return loop.is_waiting_on_delayed_task(); }, 200ms));
    const auto wake_count_before = loop.earlier_deadline_wake_count();

    loop.post_delayed_task([&]() {
        order.push_back("timer_1");
        loop.post_task([&order]() { order.push_back("render"); }, EventLoop::TaskSource::kRender);
        loop.post_task([&order, &loop]() {
            order.push_back("idle");
            loop.quit();
        }, EventLoop::TaskSource::kIdle);
        loop.post_task([&order]() { order.push_back("network"); }, EventLoop::TaskSource::kNetwork);
        loop.post_task([&order]() { order.push_back("input"); }, EventLoop::TaskSource::kInput);
        loop.post_delayed_task([&order]() { order.push_back("timer_2"); }, 0ms);
    }, 0ms);

    runner.join();

    EXPECT_EQ(loop.earlier_deadline_wake_count(), wake_count_before + 1);
    EXPECT_EQ(order, (std::vector<std::string>{
        "timer_1",
        "input",
        "network",
        "timer_2",
        "render",
        "idle",
    }));
}

TEST(EventLoopTest, EarlierDelayedWakePromotesInsertedDelayedTasksOnceInDeadlineOrder) {
    EventLoop loop;
    std::vector<std::string> order;
    std::atomic<int> first_runs{0};
    std::atomic<int> second_runs{0};
    constexpr auto kOriginalDelay = 3s;

    loop.post_delayed_task([]() {}, kOriginalDelay);
    std::thread runner([&loop]() {
        loop.run();
    });

    ASSERT_TRUE(wait_until_true([&loop]() { return loop.is_running(); }, 200ms));
    ASSERT_TRUE(wait_until_true([&loop]() { return loop.is_waiting_on_delayed_task(); }, 200ms));

    loop.post_delayed_task([&]() {
        second_runs.fetch_add(1, std::memory_order_relaxed);
        order.push_back("second");
        loop.quit();
    }, 40ms);
    loop.post_delayed_task([&]() {
        first_runs.fetch_add(1, std::memory_order_relaxed);
        order.push_back("first");
    }, 0ms);

    runner.join();

    EXPECT_EQ(first_runs.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(second_runs.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(order, (std::vector<std::string>{
        "first",
        "second",
    }));
}

TEST(EventLoopTest, EarlierDeadlineWakeTelemetryRecordsInterruptedWait) {
    EventLoop loop;
    std::atomic<bool> fired{false};
    constexpr auto kOriginalDelay = 3s;

    loop.post_delayed_task([]() {}, kOriginalDelay);
    std::thread runner([&loop]() {
        loop.run();
    });

    ASSERT_TRUE(wait_until_true([&loop]() { return loop.is_running(); }, 200ms));
    ASSERT_TRUE(wait_until_true([&loop]() { return loop.is_waiting_on_delayed_task(); }, 200ms));

    std::this_thread::sleep_for(15ms);
    const auto wake_count_before = loop.earlier_deadline_wake_count();
    loop.post_delayed_task([&]() {
        fired.store(true, std::memory_order_relaxed);
        loop.quit();
    }, 0ms);

    const bool fired_in_time =
        wait_until_true([&fired]() { return fired.load(std::memory_order_relaxed); }, 200ms);
    if (!fired_in_time) {
        loop.quit();
    }
    runner.join();

    ASSERT_TRUE(fired_in_time);
    EXPECT_FALSE(loop.is_waiting_on_delayed_task());
    EXPECT_EQ(loop.earlier_deadline_wake_count(), wake_count_before + 1);
    EXPECT_GE(loop.last_delayed_wait_duration(), 10ms);
    EXPECT_LT(loop.last_delayed_wait_duration(), 500ms);
}

TEST(EventLoopTest, RecordsLagForLateDelayedTask) {
    EventLoop loop;
    bool executed = false;

    loop.post_delayed_task([&executed]() { executed = true; }, 20ms);

    std::this_thread::sleep_for(80ms);
    loop.run_pending();

    EXPECT_TRUE(executed);
    EXPECT_GE(loop.last_delayed_task_lag(), 40ms);
    EXPECT_LT(loop.last_delayed_task_lag(), 500ms);
}

TEST(EventLoopTest, CountsLateDelayedTaskStarvation) {
    EventLoop loop;
    bool executed = false;

    loop.post_delayed_task([&executed]() { executed = true; }, 20ms);
    EXPECT_EQ(loop.delayed_task_starvation_count(), 0u);

    std::this_thread::sleep_for(80ms);
    loop.run_pending();

    EXPECT_TRUE(executed);
    EXPECT_GE(loop.delayed_task_starvation_count(), 1u);
}

TEST(EventLoopTest, RecordsLastDelayedWaitDuration) {
    EventLoop loop;
    std::thread runner([&loop]() {
        loop.run();
    });

    loop.post_delayed_task([&]() {
        loop.quit();
    }, 40ms);

    runner.join();

    EXPECT_GE(loop.last_delayed_wait_duration(), 30ms);
    EXPECT_LT(loop.last_delayed_wait_duration(), 500ms);
}

TEST(EventLoopTest, SameDeadlineDelayedTasksPreserveFIFOOrder) {
    EventLoop loop;
    std::vector<int> order;

    loop.post_delayed_task([&order]() { order.push_back(1); }, 30ms);
    loop.post_delayed_task([&order]() { order.push_back(2); }, 30ms);
    loop.post_delayed_task([&order]() { order.push_back(3); }, 30ms);

    std::this_thread::sleep_for(80ms);
    loop.run_pending();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

TEST(EventLoopTest, TaskSourcePriorityOrderIsDeterministic) {
    EventLoop loop;
    std::vector<std::string> order;

    loop.post_task([&order]() { order.push_back("idle"); }, EventLoop::TaskSource::kIdle);
    loop.post_task([&order]() { order.push_back("network"); }, EventLoop::TaskSource::kNetwork);
    loop.post_task([&order]() { order.push_back("render"); }, EventLoop::TaskSource::kRender);
    loop.post_task([&order]() { order.push_back("timer"); }, EventLoop::TaskSource::kTimer);
    loop.post_task([&order]() { order.push_back("input"); }, EventLoop::TaskSource::kInput);

    loop.run_pending();

    ASSERT_EQ(order.size(), 5u);
    EXPECT_EQ(order[0], "input");
    EXPECT_EQ(order[1], "network");
    EXPECT_EQ(order[2], "timer");
    EXPECT_EQ(order[3], "render");
    EXPECT_EQ(order[4], "idle");
}

TEST(EventLoopTest, RunPendingDrainsNestedPostsInPriorityOrder) {
    EventLoop loop;
    std::vector<std::string> order;

    loop.post_task([&order, &loop]() {
        order.push_back("outer_input");
        loop.post_task([&order]() { order.push_back("nested_input"); }, EventLoop::TaskSource::kInput);
        loop.post_task([&order]() { order.push_back("nested_network"); },
                       EventLoop::TaskSource::kNetwork);
        loop.post_task([&order]() { order.push_back("nested_idle"); }, EventLoop::TaskSource::kIdle);
        loop.post_delayed_task([&order]() { order.push_back("nested_timer"); }, 0ms);
    }, EventLoop::TaskSource::kInput);

    loop.post_task([&order]() { order.push_back("queued_network"); }, EventLoop::TaskSource::kNetwork);
    loop.post_task([&order]() { order.push_back("queued_render"); }, EventLoop::TaskSource::kRender);

    loop.run_pending();

    ASSERT_EQ(order.size(), 7u);
    EXPECT_EQ(order[0], "outer_input");
    EXPECT_EQ(order[1], "nested_input");
    EXPECT_EQ(order[2], "queued_network");
    EXPECT_EQ(order[3], "nested_network");
    EXPECT_EQ(order[4], "nested_timer");
    EXPECT_EQ(order[5], "queued_render");
    EXPECT_EQ(order[6], "nested_idle");
}

TEST(EventLoopTest, RunDrainsNestedPostsInPriorityOrder) {
    EventLoop loop;
    std::vector<std::string> order;

    loop.post_task([&order, &loop]() {
        order.push_back("outer_input");
        loop.post_task([&order]() { order.push_back("nested_input"); }, EventLoop::TaskSource::kInput);
        loop.post_task([&order]() { order.push_back("nested_network"); },
                       EventLoop::TaskSource::kNetwork);
        loop.post_task([&order, &loop]() {
            order.push_back("nested_idle");
            loop.quit();
        }, EventLoop::TaskSource::kIdle);
        loop.post_delayed_task([&order]() { order.push_back("nested_timer"); }, 0ms);
    }, EventLoop::TaskSource::kInput);

    loop.post_task([&order]() { order.push_back("queued_network"); }, EventLoop::TaskSource::kNetwork);
    loop.post_task([&order]() { order.push_back("queued_render"); }, EventLoop::TaskSource::kRender);

    std::thread runner([&loop]() {
        loop.run();
    });

    runner.join();

    ASSERT_EQ(order.size(), 7u);
    EXPECT_EQ(order[0], "outer_input");
    EXPECT_EQ(order[1], "nested_input");
    EXPECT_EQ(order[2], "queued_network");
    EXPECT_EQ(order[3], "nested_network");
    EXPECT_EQ(order[4], "nested_timer");
    EXPECT_EQ(order[5], "queued_render");
    EXPECT_EQ(order[6], "nested_idle");
}

TEST(EventLoopTest, RunAndRunPendingMatchMixedSourcePriorityOrder) {
    auto exercise = [](bool use_blocking_run) {
        EventLoop loop;
        std::vector<std::string> order;

        loop.post_task([&order, &loop, use_blocking_run]() {
            order.push_back("outer_input");
            loop.post_task([&order]() { order.push_back("nested_input"); }, EventLoop::TaskSource::kInput);
            loop.post_task([&order]() { order.push_back("nested_network"); },
                           EventLoop::TaskSource::kNetwork);
            loop.post_task([&order]() { order.push_back("nested_render"); },
                           EventLoop::TaskSource::kRender);
            loop.post_task([&order, &loop, use_blocking_run]() {
                order.push_back("nested_idle");
                if (use_blocking_run) {
                    loop.quit();
                }
            }, EventLoop::TaskSource::kIdle);
            loop.post_delayed_task([&order]() { order.push_back("nested_timer"); }, 0ms);
        }, EventLoop::TaskSource::kInput);

        loop.post_task([&order]() { order.push_back("queued_network"); }, EventLoop::TaskSource::kNetwork);
        loop.post_task([&order]() { order.push_back("queued_render"); }, EventLoop::TaskSource::kRender);
        loop.post_task([&order]() { order.push_back("queued_idle"); }, EventLoop::TaskSource::kIdle);

        if (use_blocking_run) {
            std::thread runner([&loop]() {
                loop.run();
            });
            runner.join();
        } else {
            loop.run_pending();
        }

        return order;
    };

    const auto run_pending_order = exercise(false);
    const auto run_order = exercise(true);
    const std::vector<std::string> expected_order = {
        "outer_input",
        "nested_input",
        "queued_network",
        "nested_network",
        "nested_timer",
        "queued_render",
        "nested_render",
        "queued_idle",
        "nested_idle",
    };

    EXPECT_EQ(run_pending_order, expected_order);
    EXPECT_EQ(run_order, expected_order);
}

TEST(EventLoopTest, RunAndRunPendingMatchReadyZeroDelayTimerPriorityOrder) {
    auto exercise = [](bool use_blocking_run) {
        EventLoop loop;
        std::vector<std::string> order;

        loop.post_delayed_task([&order]() { order.push_back("timer"); }, 0ms);
        std::this_thread::sleep_for(2ms);

        loop.post_task([&order]() { order.push_back("render"); }, EventLoop::TaskSource::kRender);
        loop.post_task([&order, &loop, use_blocking_run]() {
            order.push_back("idle");
            if (use_blocking_run) {
                loop.quit();
            }
        }, EventLoop::TaskSource::kIdle);
        loop.post_task([&order]() { order.push_back("network"); }, EventLoop::TaskSource::kNetwork);
        loop.post_task([&order]() {
            order.push_back("input");
        }, EventLoop::TaskSource::kInput);

        if (use_blocking_run) {
            std::thread runner([&loop]() {
                loop.run();
            });
            runner.join();
        } else {
            loop.run_pending();
        }

        return order;
    };

    const auto run_pending_order = exercise(false);
    const auto run_order = exercise(true);
    const std::vector<std::string> expected_order = {
        "input",
        "network",
        "timer",
        "render",
        "idle",
    };

    EXPECT_EQ(run_pending_order, expected_order);
    EXPECT_EQ(run_order, expected_order);
}

TEST(EventLoopTest, PromotedZeroDelayTimerKeepsTimerSourceFIFO) {
    EventLoop loop;
    std::vector<std::string> order;

    loop.post_task([&order, &loop]() {
        order.push_back("outer_input");
        loop.post_delayed_task([&order]() { order.push_back("promoted_timer"); }, 0ms);
    }, EventLoop::TaskSource::kInput);
    loop.post_task([&order]() { order.push_back("queued_timer"); }, EventLoop::TaskSource::kTimer);
    loop.post_task([&order]() { order.push_back("queued_render"); }, EventLoop::TaskSource::kRender);

    loop.run_pending();

    ASSERT_EQ(order.size(), 4u);
    EXPECT_EQ(order[0], "outer_input");
    EXPECT_EQ(order[1], "queued_timer");
    EXPECT_EQ(order[2], "promoted_timer");
    EXPECT_EQ(order[3], "queued_render");
}

TEST(EventLoopTest, ResumedDelayedTaskNestedPostsReenterBySourcePriority) {
    EventLoop loop;
    std::vector<std::string> order;
    constexpr auto kOriginalDelay = 3s;

    loop.post_delayed_task([]() {}, kOriginalDelay);
    std::thread runner([&loop]() {
        loop.run();
    });

    ASSERT_TRUE(wait_until_true([&loop]() { return loop.is_running(); }, 200ms));
    ASSERT_TRUE(wait_until_true([&loop]() { return loop.is_waiting_on_delayed_task(); }, 200ms));

    loop.post_delayed_task([&]() {
        order.push_back("timer_1");
        loop.post_task([&order]() { order.push_back("input_1"); }, EventLoop::TaskSource::kInput);
        loop.post_task([&order]() { order.push_back("input_2"); }, EventLoop::TaskSource::kInput);
        loop.post_task([&order]() { order.push_back("network_1"); }, EventLoop::TaskSource::kNetwork);
        loop.post_task([&order]() { order.push_back("network_2"); }, EventLoop::TaskSource::kNetwork);
        loop.post_task([&order]() { order.push_back("render_1"); }, EventLoop::TaskSource::kRender);
        loop.post_task([&order]() { order.push_back("render_2"); }, EventLoop::TaskSource::kRender);
        loop.post_task([&order]() { order.push_back("idle_1"); }, EventLoop::TaskSource::kIdle);
        loop.post_task([&order, &loop]() {
            order.push_back("idle_2");
            loop.quit();
        }, EventLoop::TaskSource::kIdle);
        loop.post_delayed_task([&order]() { order.push_back("timer_2"); }, 0ms);
    }, 0ms);

    runner.join();

    EXPECT_EQ(order, (std::vector<std::string>{
        "timer_1",
        "input_1",
        "input_2",
        "network_1",
        "network_2",
        "timer_2",
        "render_1",
        "render_2",
        "idle_1",
        "idle_2",
    }));
}

TEST(EventLoopTest, ResumedDelayedTaskReentrantSameSourcePostsStayFIFO) {
    EventLoop loop;
    std::vector<std::string> order;
    constexpr auto kOriginalDelay = 3s;

    loop.post_delayed_task([]() {}, kOriginalDelay);
    std::thread runner([&loop]() {
        loop.run();
    });

    ASSERT_TRUE(wait_until_true([&loop]() { return loop.is_running(); }, 200ms));
    ASSERT_TRUE(wait_until_true([&loop]() { return loop.is_waiting_on_delayed_task(); }, 200ms));

    loop.post_delayed_task([&]() {
        order.push_back("timer");
        loop.post_task([&order]() { order.push_back("input"); }, EventLoop::TaskSource::kInput);
        loop.post_task([&order, &loop]() {
            order.push_back("network_1");
            loop.post_task([&order]() { order.push_back("network_3"); },
                           EventLoop::TaskSource::kNetwork);
        }, EventLoop::TaskSource::kNetwork);
        loop.post_task([&order]() { order.push_back("network_2"); }, EventLoop::TaskSource::kNetwork);
        loop.post_task([&order]() { order.push_back("render"); }, EventLoop::TaskSource::kRender);
        loop.post_task([&order, &loop]() {
            order.push_back("idle");
            loop.quit();
        }, EventLoop::TaskSource::kIdle);
    }, 0ms);

    runner.join();

    EXPECT_EQ(order, (std::vector<std::string>{
        "timer",
        "input",
        "network_1",
        "network_2",
        "network_3",
        "render",
        "idle",
    }));
}

TEST(EventLoopTest, TaskSourceFIFOWithinSameSourceAndLowerPriorityDefers) {
    EventLoop loop;
    std::vector<int> order;

    loop.post_task([&order]() { order.push_back(1); }, EventLoop::TaskSource::kNetwork);
    loop.post_task([&order]() { order.push_back(2); }, EventLoop::TaskSource::kNetwork);
    loop.post_task([&order]() { order.push_back(3); }, EventLoop::TaskSource::kInput);
    loop.post_task([&order]() { order.push_back(4); }, EventLoop::TaskSource::kNetwork);
    loop.post_task([&order]() { order.push_back(5); }, EventLoop::TaskSource::kInput);

    loop.run_pending();

    ASSERT_EQ(order.size(), 5u);
    EXPECT_EQ(order[0], 3);
    EXPECT_EQ(order[1], 5);
    EXPECT_EQ(order[2], 1);
    EXPECT_EQ(order[3], 2);
    EXPECT_EQ(order[4], 4);
}

TEST(EventLoopTest, PostTaskDefaultSourceMatchesInputPriority) {
    EventLoop loop;
    std::vector<int> order;

    loop.post_task([&order]() { order.push_back(2); }, EventLoop::TaskSource::kNetwork);
    loop.post_task([&order]() { order.push_back(1); });
    loop.post_task([&order]() { order.push_back(3); }, EventLoop::TaskSource::kIdle);

    loop.run_pending();

    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

// ---------------------------------------------------------------------------
// Cycle 436 — is_running initial, quit-before-run, pending_count with delayed,
//             non-due skip, zero-delay, is_running inside task,
//             combined pending count, concurrent multi-thread post
// ---------------------------------------------------------------------------

TEST(EventLoopTest, IsRunningFalseInitially) {
    EventLoop loop;
    EXPECT_FALSE(loop.is_running());
}

TEST(EventLoopTest, IsRunningFalseAfterRunCompletes) {
    // is_running() should return false after run() returns via quit()
    EventLoop loop;
    loop.post_task([&loop]() { loop.quit(); });
    loop.run();
    EXPECT_FALSE(loop.is_running());
}

TEST(EventLoopTest, PendingCountIncludesDelayedTasks) {
    EventLoop loop;
    EXPECT_EQ(loop.pending_count(), 0u);

    loop.post_delayed_task([]() {}, 1000ms);  // not due for 1 second
    EXPECT_EQ(loop.pending_count(), 1u);

    loop.post_task([]() {});
    EXPECT_EQ(loop.pending_count(), 2u);
}

TEST(EventLoopTest, RunPendingSkipsNonDueDelayedTasks) {
    EventLoop loop;
    bool executed = false;

    loop.post_delayed_task([&executed]() { executed = true; }, 500ms);
    loop.run_pending();  // task not due, should not execute

    EXPECT_FALSE(executed);
    EXPECT_EQ(loop.pending_count(), 1u);  // still pending
}

TEST(EventLoopTest, ZeroDelayTaskFiresInRunPending) {
    EventLoop loop;
    bool executed = false;

    loop.post_delayed_task([&executed]() { executed = true; }, 0ms);
    loop.run_pending();  // 0ms delay: run_at <= now is satisfied

    EXPECT_TRUE(executed) << "Zero-delay task should fire in the same run_pending call";
}

TEST(EventLoopTest, IsRunningTrueDuringRun) {
    EventLoop loop;
    bool was_running = false;

    loop.post_task([&loop, &was_running]() {
        was_running = loop.is_running();
        loop.quit();
    });

    loop.run();
    EXPECT_TRUE(was_running) << "is_running() should return true while run() is executing";
}

TEST(EventLoopTest, PendingCountCombinesImmediateAndDelayed) {
    EventLoop loop;
    loop.post_task([]() {});
    loop.post_task([]() {});
    loop.post_delayed_task([]() {}, 500ms);
    loop.post_delayed_task([]() {}, 500ms);

    EXPECT_EQ(loop.pending_count(), 4u)
        << "pending_count() should sum immediate (2) and delayed (2) tasks";
}

TEST(EventLoopTest, ConcurrentPostFromMultipleThreads) {
    EventLoop loop;
    std::atomic<int> counter{0};

    const int kThreads = 4;
    const int kTasksPerThread = 10;

    std::vector<std::thread> threads;
    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&loop, &counter]() {
            for (int i = 0; i < kTasksPerThread; ++i) {
                loop.post_task([&counter]() {
                    counter.fetch_add(1, std::memory_order_relaxed);
                });
            }
        });
    }
    for (auto& th : threads) th.join();

    loop.run_pending();
    EXPECT_EQ(counter.load(), kThreads * kTasksPerThread)
        << "All " << kThreads * kTasksPerThread << " tasks from concurrent threads should execute";
}

// ---------------------------------------------------------------------------
// Cycle 492 — event loop further edge-case regression tests
// ---------------------------------------------------------------------------

// After run() ends via quit(), run_pending() still drains newly posted tasks
TEST(EventLoopTest, PostTaskAfterQuitDrainedByRunPending) {
    EventLoop loop;
    loop.post_task([&loop]() { loop.quit(); });
    loop.run(); // quits immediately

    bool executed = false;
    loop.post_task([&executed]() { executed = true; });
    loop.run_pending();
    EXPECT_TRUE(executed);
}

// Multiple run_pending calls on the same loop each drain their own batch
TEST(EventLoopTest, MultipleRunPendingCallsDrainSeparateBatches) {
    EventLoop loop;
    int count = 0;

    loop.post_task([&count]() { count += 1; });
    loop.post_task([&count]() { count += 1; });
    loop.run_pending(); // drain first batch of 2
    EXPECT_EQ(count, 2);

    loop.post_task([&count]() { count += 10; });
    loop.post_task([&count]() { count += 10; });
    loop.run_pending(); // drain second batch of 2
    EXPECT_EQ(count, 22);
}

// pending_count reaches 0 after a delayed task fires
TEST(EventLoopTest, DelayedPendingCountZeroAfterFiring) {
    EventLoop loop;
    loop.post_delayed_task([]() {}, 50ms);
    EXPECT_EQ(loop.pending_count(), 1u);

    std::this_thread::sleep_for(100ms);
    loop.run_pending();
    EXPECT_EQ(loop.pending_count(), 0u);
}

// Immediate task runs; far-future delayed task stays pending in same run_pending
TEST(EventLoopTest, ImmediateTaskRunsFarFutureDelayedStaysPending) {
    EventLoop loop;
    bool immediate_ran = false;
    bool delayed_ran = false;

    loop.post_task([&immediate_ran]() { immediate_ran = true; });
    loop.post_delayed_task([&delayed_ran]() { delayed_ran = true; }, 5000ms);

    loop.run_pending();

    EXPECT_TRUE(immediate_ran);
    EXPECT_FALSE(delayed_ran);
    EXPECT_EQ(loop.pending_count(), 1u); // delayed task still pending
}

// post_task accepts a lambda that captures a local variable by value
TEST(EventLoopTest, PostTaskCapturingValueByValue) {
    EventLoop loop;
    int captured_val = 99;
    int result = 0;

    loop.post_task([captured_val, &result]() { result = captured_val; });
    captured_val = 0; // change after capture — result should still be 99
    loop.run_pending();

    EXPECT_EQ(result, 99);
}

// A large number of tasks all execute via run_pending
TEST(EventLoopTest, LargeNumberOfTasksAllExecute) {
    EventLoop loop;
    std::atomic<int> counter{0};

    const int kCount = 1000;
    for (int i = 0; i < kCount; ++i) {
        loop.post_task([&counter]() { counter.fetch_add(1, std::memory_order_relaxed); });
    }

    loop.run_pending();
    EXPECT_EQ(counter.load(), kCount);
}

// A delayed task posted from within a running task fires on a later run_pending
TEST(EventLoopTest, PostDelayedFromWithinTaskFiresLater) {
    EventLoop loop;
    bool inner_fired = false;

    loop.post_task([&loop, &inner_fired]() {
        loop.post_delayed_task([&inner_fired]() { inner_fired = true; }, 50ms);
    });

    loop.run_pending(); // runs outer task, enqueues delayed inner
    EXPECT_FALSE(inner_fired);

    std::this_thread::sleep_for(100ms);
    loop.run_pending(); // inner task now due
    EXPECT_TRUE(inner_fired);
}

// Tasks execute in FIFO order even when interspersed with delayed (non-due) tasks
TEST(EventLoopTest, FIFOOrderPreservedWithDelayedInterspersed) {
    EventLoop loop;
    std::vector<int> order;

    loop.post_task([&order]() { order.push_back(1); });
    loop.post_delayed_task([&order]() { order.push_back(99); }, 5000ms); // will not fire
    loop.post_task([&order]() { order.push_back(2); });
    loop.post_task([&order]() { order.push_back(3); });

    loop.run_pending();

    ASSERT_EQ(order.size(), 3u); // delayed task not included
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
}

// ============================================================================
// Cycle 507: EventLoop regression tests
// ============================================================================

// pending_count() drops to 0 after run_pending drains all tasks
TEST(EventLoopTest, PendingCountDropsToZeroAfterRunPending) {
    EventLoop loop;
    loop.post_task([]() {});
    loop.post_task([]() {});
    loop.post_task([]() {});
    EXPECT_EQ(loop.pending_count(), 3u);
    loop.run_pending();
    EXPECT_EQ(loop.pending_count(), 0u);
}

// run_pending() with an empty queue does nothing and doesn't crash
TEST(EventLoopTest, RunPendingWithEmptyQueueIsNoOp) {
    EventLoop loop;
    EXPECT_EQ(loop.pending_count(), 0u);
    EXPECT_NO_THROW(loop.run_pending());
    EXPECT_EQ(loop.pending_count(), 0u);
}

// lambda captures std::string by value, result is correct
TEST(EventLoopTest, PostTaskCapturingStdString) {
    EventLoop loop;
    std::string expected = "hello from task";
    std::string result;
    loop.post_task([captured = expected, &result]() { result = captured; });
    expected = "changed";  // change original — should not affect captured
    loop.run_pending();
    EXPECT_EQ(result, "hello from task");
}

// 5 tasks each increment a counter — total should be 5
TEST(EventLoopTest, FiveTasksSumToFive) {
    EventLoop loop;
    int counter = 0;
    for (int i = 0; i < 5; ++i) {
        loop.post_task([&counter]() { counter++; });
    }
    loop.run_pending();
    EXPECT_EQ(counter, 5);
}

// post 7 tasks, pending_count is 7 before running
TEST(EventLoopTest, PendingCountCorrectAfterBatchPost) {
    EventLoop loop;
    for (int i = 0; i < 7; ++i) {
        loop.post_task([]() {});
    }
    EXPECT_EQ(loop.pending_count(), 7u);
}

// task posted but run_pending never called — task not executed
TEST(EventLoopTest, PostedTaskNotExecutedWithoutRunPending) {
    EventLoop loop;
    bool executed = false;
    loop.post_task([&executed]() { executed = true; });
    // Do NOT call run_pending
    EXPECT_FALSE(executed);
    EXPECT_EQ(loop.pending_count(), 1u);
}

// alternating post + run_pending pattern
TEST(EventLoopTest, AlternatingPostAndRunPending) {
    EventLoop loop;
    int counter = 0;
    loop.post_task([&counter]() { counter += 1; });
    loop.run_pending();
    EXPECT_EQ(counter, 1);
    loop.post_task([&counter]() { counter += 10; });
    loop.run_pending();
    EXPECT_EQ(counter, 11);
    loop.post_task([&counter]() { counter += 100; });
    loop.run_pending();
    EXPECT_EQ(counter, 111);
}

// delayed task with 1-hour delay is not executed by run_pending
TEST(EventLoopTest, LongDelayedTaskNotRunByRunPending) {
    EventLoop loop;
    bool executed = false;
    loop.post_delayed_task([&executed]() { executed = true; }, std::chrono::hours(1));
    loop.run_pending();  // non-due delayed task should NOT fire
    EXPECT_FALSE(executed);
}
