#include <clever/platform/event_loop.h>

#include <gtest/gtest.h>

#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

using namespace clever::platform;
using namespace std::chrono_literals;

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
// 8. Post task from within a task
// ---------------------------------------------------------------------------
TEST(EventLoopTest, PostTaskFromWithinATask) {
    EventLoop loop;
    bool inner_executed = false;

    loop.post_task([&loop, &inner_executed]() {
        loop.post_task([&inner_executed]() {
            inner_executed = true;
        });
    });

    // First run_pending executes the outer task (which posts the inner task)
    loop.run_pending();

    // The inner task should now be pending
    EXPECT_EQ(loop.pending_count(), 1u);

    // Second run_pending executes the inner task
    loop.run_pending();
    EXPECT_TRUE(inner_executed);
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

    // Give run() time to start and block
    std::this_thread::sleep_for(50ms);
    EXPECT_TRUE(loop.is_running());

    // Post a task that sets the flag and then quits
    loop.post_task([&task_executed, &loop]() {
        task_executed.store(true);
        loop.quit();
    });

    runner.join();

    EXPECT_TRUE(task_executed.load());
    EXPECT_FALSE(loop.is_running());
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
