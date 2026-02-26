#include <clever/platform/thread_pool.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <numeric>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

using namespace clever::platform;
using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// 1. Construction with default thread count
// ---------------------------------------------------------------------------
TEST(ThreadPoolTest, ConstructWithDefaultThreadCount) {
    ThreadPool pool;
    EXPECT_EQ(pool.size(), std::thread::hardware_concurrency());
    EXPECT_TRUE(pool.is_running());
}

// ---------------------------------------------------------------------------
// 2. Construction with explicit thread count
// ---------------------------------------------------------------------------
TEST(ThreadPoolTest, ConstructWithExplicitThreadCount) {
    constexpr size_t kThreads = 4;
    ThreadPool pool(kThreads);
    EXPECT_EQ(pool.size(), kThreads);
    EXPECT_TRUE(pool.is_running());
}

// ---------------------------------------------------------------------------
// 3. Submit task and get result via future
// ---------------------------------------------------------------------------
TEST(ThreadPoolTest, SubmitTaskAndGetResult) {
    ThreadPool pool(2);
    auto future = pool.submit([]() { return 42; });
    EXPECT_EQ(future.get(), 42);
}

// ---------------------------------------------------------------------------
// 4. Submit multiple tasks in parallel
// ---------------------------------------------------------------------------
TEST(ThreadPoolTest, SubmitMultipleTasksInParallel) {
    ThreadPool pool(4);

    constexpr int kTasks = 100;
    std::vector<std::future<int>> futures;
    futures.reserve(kTasks);

    for (int i = 0; i < kTasks; ++i) {
        futures.push_back(pool.submit([i]() { return i * i; }));
    }

    for (int i = 0; i < kTasks; ++i) {
        EXPECT_EQ(futures[i].get(), i * i);
    }
}

// ---------------------------------------------------------------------------
// 5. Post fire-and-forget task
// ---------------------------------------------------------------------------
TEST(ThreadPoolTest, PostFireAndForgetTask) {
    ThreadPool pool(2);
    std::atomic<bool> executed{false};

    pool.post([&executed]() { executed.store(true); });

    // Give it a moment to execute
    auto deadline = std::chrono::steady_clock::now() + 2s;
    while (!executed.load() && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_TRUE(executed.load());
}

// ---------------------------------------------------------------------------
// 6. Tasks execute on different threads
// ---------------------------------------------------------------------------
TEST(ThreadPoolTest, TasksExecuteOnDifferentThreads) {
    constexpr size_t kThreads = 4;
    ThreadPool pool(kThreads);

    std::mutex ids_mutex;
    std::set<std::thread::id> thread_ids;
    std::atomic<int> started{0};
    std::atomic<bool> go{false};

    // Submit tasks that block until all have started, ensuring they run in
    // parallel on different threads.
    std::vector<std::future<void>> futures;
    for (size_t i = 0; i < kThreads; ++i) {
        futures.push_back(pool.submit([&]() {
            {
                std::lock_guard lock(ids_mutex);
                thread_ids.insert(std::this_thread::get_id());
            }
            started.fetch_add(1);
            while (!go.load()) {
                std::this_thread::sleep_for(1ms);
            }
        }));
    }

    // Wait until all tasks have started
    auto deadline = std::chrono::steady_clock::now() + 5s;
    while (started.load() < static_cast<int>(kThreads) &&
           std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_EQ(started.load(), static_cast<int>(kThreads));

    go.store(true);
    for (auto& f : futures) {
        f.get();
    }

    // We should have observed at least 2 distinct thread ids (though typically
    // it will be kThreads).
    EXPECT_GE(thread_ids.size(), 2u);
}

// ---------------------------------------------------------------------------
// 7. Shutdown waits for pending tasks
// ---------------------------------------------------------------------------
TEST(ThreadPoolTest, ShutdownWaitsForPendingTasks) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};

    for (int i = 0; i < 50; ++i) {
        pool.post([&counter]() {
            std::this_thread::sleep_for(1ms);
            counter.fetch_add(1);
        });
    }

    pool.shutdown();
    EXPECT_EQ(counter.load(), 50);
    EXPECT_FALSE(pool.is_running());
}

// ---------------------------------------------------------------------------
// 8. Submit after shutdown throws
// ---------------------------------------------------------------------------
TEST(ThreadPoolTest, SubmitAfterShutdownThrows) {
    ThreadPool pool(2);
    pool.shutdown();

    EXPECT_THROW(pool.submit([]() { return 1; }), std::runtime_error);
}

// ---------------------------------------------------------------------------
// 9. Destructor calls shutdown
// ---------------------------------------------------------------------------
TEST(ThreadPoolTest, DestructorCallsShutdown) {
    std::atomic<int> counter{0};

    {
        ThreadPool pool(2);
        for (int i = 0; i < 20; ++i) {
            pool.post([&counter]() {
                std::this_thread::sleep_for(1ms);
                counter.fetch_add(1);
            });
        }
        // pool goes out of scope here — destructor should call shutdown and
        // wait for all tasks.
    }

    EXPECT_EQ(counter.load(), 20);
}

// ---------------------------------------------------------------------------
// 10. Thread pool with 1 thread (sequential execution)
// ---------------------------------------------------------------------------
TEST(ThreadPoolTest, SingleThreadSequentialExecution) {
    ThreadPool pool(1);

    std::vector<int> order;
    std::mutex order_mutex;

    constexpr int kTasks = 20;
    std::vector<std::future<void>> futures;
    for (int i = 0; i < kTasks; ++i) {
        futures.push_back(pool.submit([i, &order, &order_mutex]() {
            std::lock_guard lock(order_mutex);
            order.push_back(i);
        }));
    }

    for (auto& f : futures) {
        f.get();
    }

    // With a single thread the tasks must execute in submission order.
    ASSERT_EQ(order.size(), static_cast<size_t>(kTasks));
    for (int i = 0; i < kTasks; ++i) {
        EXPECT_EQ(order[i], i);
    }
}

// ---------------------------------------------------------------------------
// 11. Submit task that throws — exception captured in future
// ---------------------------------------------------------------------------
TEST(ThreadPoolTest, SubmitTaskThatThrowsExceptionCapturedInFuture) {
    ThreadPool pool(2);

    auto future = pool.submit([]() -> int {
        throw std::logic_error("test exception");
    });

    EXPECT_THROW(future.get(), std::logic_error);
}

// ---------------------------------------------------------------------------
// Cycle 483 — additional ThreadPool regression tests
// ---------------------------------------------------------------------------

// 12. Submit void task — future<void> is usable
TEST(ThreadPoolTest, SubmitVoidTask) {
    ThreadPool pool(2);
    auto future = pool.submit([]() { /* nothing */ });
    EXPECT_NO_THROW(future.get());
}

// 13. Submit task returning std::string
TEST(ThreadPoolTest, SubmitTaskReturningString) {
    ThreadPool pool(2);
    auto future = pool.submit([]() -> std::string { return "hello pool"; });
    EXPECT_EQ(future.get(), "hello pool");
}

// 14. Multiple concurrent tasks atomically increment a counter
TEST(ThreadPoolTest, ConcurrentAtomicCounterIncrement) {
    constexpr int kTasks = 200;
    ThreadPool pool(4);
    std::atomic<int> counter{0};

    std::vector<std::future<void>> futures;
    futures.reserve(kTasks);
    for (int i = 0; i < kTasks; ++i) {
        futures.push_back(pool.submit([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        }));
    }
    for (auto& f : futures) f.get();

    EXPECT_EQ(counter.load(), kTasks);
}

// 15. Multiple shutdown() calls are safe (no crash / double-free)
TEST(ThreadPoolTest, MultipleShutdownCallsAreSafe) {
    ThreadPool pool(2);
    EXPECT_NO_THROW({
        pool.shutdown();
        pool.shutdown(); // idempotent
    });
    EXPECT_FALSE(pool.is_running());
}

// 16. Interleave post() and submit() — all tasks execute
TEST(ThreadPoolTest, PostAndSubmitInterleavedExecution) {
    ThreadPool pool(3);
    std::atomic<int> count{0};

    std::vector<std::future<void>> futures;
    for (int i = 0; i < 20; ++i) {
        if (i % 2 == 0) {
            pool.post([&count]() { count.fetch_add(1); });
        } else {
            futures.push_back(pool.submit([&count]() { count.fetch_add(1); }));
        }
    }
    for (auto& f : futures) f.get();

    // Wait for fire-and-forget posts to complete
    auto deadline = std::chrono::steady_clock::now() + 3s;
    while (count.load() < 20 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_EQ(count.load(), 20);
}

// 17. Large batch: 500 tasks, each returns its index; verify sum
TEST(ThreadPoolTest, LargeBatchTasksVerifySum) {
    constexpr int kTasks = 500;
    ThreadPool pool(8);

    std::vector<std::future<int>> futures;
    futures.reserve(kTasks);
    for (int i = 0; i < kTasks; ++i) {
        futures.push_back(pool.submit([i]() { return i; }));
    }

    long long sum = 0;
    for (auto& f : futures) sum += f.get();

    long long expected = static_cast<long long>(kTasks - 1) * kTasks / 2;
    EXPECT_EQ(sum, expected);
}

// 18. Single-thread pool: post 30 tasks, then shutdown — all executed
TEST(ThreadPoolTest, SingleThreadPostThenShutdown) {
    ThreadPool pool(1);
    std::atomic<int> counter{0};

    for (int i = 0; i < 30; ++i) {
        pool.post([&counter]() {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            counter.fetch_add(1);
        });
    }

    pool.shutdown();
    EXPECT_EQ(counter.load(), 30);
}

// 19. Task captures a unique_ptr (move-only type)
TEST(ThreadPoolTest, SubmitTaskWithMoveOnlyCapture) {
    ThreadPool pool(2);
    auto value = std::make_unique<int>(99);

    auto future = pool.submit([v = std::move(value)]() -> int { return *v; });
    EXPECT_EQ(future.get(), 99);
}

// ============================================================================
// Cycle 500: ThreadPool milestone regression tests
// ============================================================================

// submit() returning double precision value
TEST(ThreadPoolTest, SubmitReturningDouble) {
    ThreadPool pool(2);
    auto future = pool.submit([]() -> double { return 3.14159; });
    EXPECT_DOUBLE_EQ(future.get(), 3.14159);
}

// submit() returning bool
TEST(ThreadPoolTest, SubmitReturningBool) {
    ThreadPool pool(2);
    auto t = pool.submit([]() -> bool { return true; });
    auto f = pool.submit([]() -> bool { return false; });
    EXPECT_TRUE(t.get());
    EXPECT_FALSE(f.get());
}

// submit(f, arg1, arg2) — args forwarded to the callable
TEST(ThreadPoolTest, SubmitWithBoundArguments) {
    ThreadPool pool(2);
    auto future = pool.submit([](int x, int y) { return x + y; }, 17, 25);
    EXPECT_EQ(future.get(), 42);
}

// ThreadPool(1) — size() == 1
TEST(ThreadPoolTest, SizeIsOneForSingleThreadPool) {
    ThreadPool pool(1);
    EXPECT_EQ(pool.size(), 1u);
    EXPECT_TRUE(pool.is_running());
}

// chain two submit calls: second task uses result of first via future
TEST(ThreadPoolTest, SubmitChainedTasks) {
    ThreadPool pool(2);
    auto first = pool.submit([]() { return 10; });
    int base = first.get();  // synchronise here
    auto second = pool.submit([base]() { return base * 3; });
    EXPECT_EQ(second.get(), 30);
}

// submit() returning a negative integer
TEST(ThreadPoolTest, SubmitTaskWithNegativeReturn) {
    ThreadPool pool(2);
    auto future = pool.submit([]() { return -42; });
    EXPECT_EQ(future.get(), -42);
}

// submit() returning a vector
TEST(ThreadPoolTest, SubmitReturningVector) {
    ThreadPool pool(2);
    auto future = pool.submit([]() -> std::vector<int> { return {1, 2, 3, 4, 5}; });
    auto result = future.get();
    ASSERT_EQ(result.size(), 5u);
    EXPECT_EQ(result[0], 1);
    EXPECT_EQ(result[4], 5);
}

// submit() returning std::pair<int,int>
TEST(ThreadPoolTest, SubmitReturningPair) {
    ThreadPool pool(2);
    auto future = pool.submit([]() { return std::make_pair(7, 13); });
    auto [a, b] = future.get();
    EXPECT_EQ(a, 7);
    EXPECT_EQ(b, 13);
}

// ============================================================================
// Cycle 524: ThreadPool regression tests
// ============================================================================

// ThreadPool with 4 threads runs 4 concurrent tasks
TEST(ThreadPoolTest, FourThreadsRunFourConcurrentTasks) {
    ThreadPool pool(4);
    std::atomic<int> count{0};
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 4; ++i) {
        futures.push_back(pool.submit([&count]() {
            count.fetch_add(1, std::memory_order_relaxed);
        }));
    }
    for (auto& f : futures) f.get();
    EXPECT_EQ(count.load(), 4);
}

// Submit returns future that can be waited on
TEST(ThreadPoolTest, SubmitFutureBlocksUntilTaskDone) {
    ThreadPool pool(2);
    bool done = false;
    auto future = pool.submit([&done]() {
        done = true;
    });
    future.get();
    EXPECT_TRUE(done);
}

// Tasks execute even when submitted rapidly
TEST(ThreadPoolTest, RapidSubmissionAllExecute) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};
    const int N = 50;
    std::vector<std::future<void>> futures;
    for (int i = 0; i < N; ++i) {
        futures.push_back(pool.submit([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        }));
    }
    for (auto& f : futures) f.get();
    EXPECT_EQ(counter.load(), N);
}

// submit with return value int
TEST(ThreadPoolTest, SubmitReturningInt) {
    ThreadPool pool(1);
    auto future = pool.submit([]() -> int { return 100; });
    EXPECT_EQ(future.get(), 100);
}

// post (fire-and-forget) runs all tasks
TEST(ThreadPoolTest, PostFireAndForgetAllRun) {
    ThreadPool pool(2);
    std::atomic<int> counter{0};
    const int N = 20;
    for (int i = 0; i < N; ++i) {
        pool.post([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }
    // Give tasks time to run
    std::this_thread::sleep_for(50ms);
    EXPECT_EQ(counter.load(), N);
}

// Thread pool size() reports correct worker count
TEST(ThreadPoolTest, SizeReportsWorkerCount) {
    ThreadPool pool(3);
    EXPECT_EQ(pool.size(), 3u);
}

// submit returning std::string
TEST(ThreadPoolTest, SubmitReturningStringValue) {
    ThreadPool pool(1);
    auto future = pool.submit([]() { return std::string("hello"); });
    EXPECT_EQ(future.get(), "hello");
}

// 10 tasks on 1-thread pool execute sequentially (all must complete)
TEST(ThreadPoolTest, TenTasksOnSingleThreadAllComplete) {
    ThreadPool pool(1);
    std::vector<int> results;
    std::mutex mu;
    std::vector<std::future<void>> futures;
    for (int i = 0; i < 10; ++i) {
        futures.push_back(pool.submit([i, &results, &mu]() {
            std::lock_guard<std::mutex> lock(mu);
            results.push_back(i);
        }));
    }
    for (auto& f : futures) f.get();
    EXPECT_EQ(results.size(), 10u);
}

// ============================================================================
// Cycle 535: ThreadPool regression tests
// ============================================================================

// Two thread pool: submit returns std::pair
TEST(ThreadPoolTest, SubmitReturnsPair) {
    ThreadPool pool(2);
    auto future = pool.submit([]() { return std::make_pair(3, 4); });
    auto [a, b] = future.get();
    EXPECT_EQ(a, 3);
    EXPECT_EQ(b, 4);
}

// Thread pool with 2 threads: is_running initially true
TEST(ThreadPoolTest, IsRunningTrueOnConstruct) {
    ThreadPool pool(2);
    EXPECT_TRUE(pool.is_running());
}

// Pool with 6 threads: size reports 6
TEST(ThreadPoolTest, SixThreadPoolReportsSix) {
    ThreadPool pool(6);
    EXPECT_EQ(pool.size(), 6u);
    EXPECT_TRUE(pool.is_running());
}

// Submit task that returns zero
TEST(ThreadPoolTest, SubmitReturnsZero) {
    ThreadPool pool(2);
    auto future = pool.submit([]() { return 0; });
    EXPECT_EQ(future.get(), 0);
}

// Submit returns long long value
TEST(ThreadPoolTest, SubmitReturningLongLong) {
    ThreadPool pool(2);
    auto future = pool.submit([]() -> long long { return 9999999999LL; });
    EXPECT_EQ(future.get(), 9999999999LL);
}

// Post 10 tasks then verify all run via shared counter
TEST(ThreadPoolTest, PostTenTasksAllRun) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    for (int i = 0; i < 10; ++i) {
        pool.post([&counter]() {
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }
    auto deadline = std::chrono::steady_clock::now() + 3s;
    while (counter.load() < 10 && std::chrono::steady_clock::now() < deadline) {
        std::this_thread::sleep_for(1ms);
    }
    EXPECT_EQ(counter.load(), 10);
}

// Submit with lambda that captures by value
TEST(ThreadPoolTest, SubmitCaptureByValue) {
    ThreadPool pool(2);
    int x = 7;
    auto future = pool.submit([x]() { return x * x; });
    EXPECT_EQ(future.get(), 49);
}

// Submit 3 tasks: all complete and values are correct
TEST(ThreadPoolTest, SubmitThreeTasksAllCorrect) {
    ThreadPool pool(3);
    auto f1 = pool.submit([]() { return 1; });
    auto f2 = pool.submit([]() { return 2; });
    auto f3 = pool.submit([]() { return 3; });
    EXPECT_EQ(f1.get() + f2.get() + f3.get(), 6);
}

// ============================================================================
// Cycle 549: ThreadPool regression tests
// ============================================================================

// Two tasks submitted concurrently, both complete
TEST(ThreadPoolTest, TwoTasksBothComplete) {
    ThreadPool pool(2);
    auto f1 = pool.submit([]() { return 11; });
    auto f2 = pool.submit([]() { return 22; });
    EXPECT_EQ(f1.get(), 11);
    EXPECT_EQ(f2.get(), 22);
}

// Submit 0 tasks: pool still running, no tasks
TEST(ThreadPoolTest, ZeroTasksPoolStillRunning) {
    ThreadPool pool(2);
    EXPECT_TRUE(pool.is_running());
    EXPECT_EQ(pool.size(), 2u);
}

// Submit after shutdown: throws runtime_error
TEST(ThreadPoolTest, PostAfterShutdownThrows) {
    ThreadPool pool(2);
    pool.shutdown();
    EXPECT_THROW(pool.post([]() {}), std::runtime_error);
}

// Submit a task that returns a char
TEST(ThreadPoolTest, SubmitReturningChar) {
    ThreadPool pool(2);
    auto future = pool.submit([]() -> char { return 'Z'; });
    EXPECT_EQ(future.get(), 'Z');
}

// Pool size 8: reports 8 threads
TEST(ThreadPoolTest, EightThreadPool) {
    ThreadPool pool(8);
    EXPECT_EQ(pool.size(), 8u);
    EXPECT_TRUE(pool.is_running());
}

// is_running becomes false after shutdown
TEST(ThreadPoolTest, IsRunningFalseAfterShutdown) {
    ThreadPool pool(2);
    EXPECT_TRUE(pool.is_running());
    pool.shutdown();
    EXPECT_FALSE(pool.is_running());
}

// Submit: task runs on a thread different from caller
TEST(ThreadPoolTest, TaskRunsOnDifferentThread) {
    ThreadPool pool(2);
    std::promise<std::thread::id> prom;
    auto fut = prom.get_future();
    pool.post([&prom]() {
        prom.set_value(std::this_thread::get_id());
    });
    auto task_thread = fut.get();
    EXPECT_NE(task_thread, std::this_thread::get_id());
}

// Submit returns future<float>
TEST(ThreadPoolTest, SubmitReturnsFloat) {
    ThreadPool pool(2);
    auto future = pool.submit([]() -> float { return 2.5f; });
    EXPECT_FLOAT_EQ(future.get(), 2.5f);
}

// ============================================================================
// Cycle 569: More ThreadPool tests
// ============================================================================

// Submit 100 tasks, all complete
TEST(ThreadPoolTest, Submit100TasksAllComplete) {
    ThreadPool pool(4);
    std::atomic<int> count{0};
    for (int i = 0; i < 100; ++i) {
        pool.post([&count]() { count.fetch_add(1); });
    }
    pool.shutdown();
    EXPECT_EQ(count.load(), 100);
}

// Pool size 1: single thread still works
TEST(ThreadPoolTest, SingleThreadPoolWorks) {
    ThreadPool pool(1);
    auto fut = pool.submit([]() { return 42; });
    EXPECT_EQ(fut.get(), 42);
}

// Pool size 3: reports 3 threads
TEST(ThreadPoolTest, ThreeThreadPoolReportsThree) {
    ThreadPool pool(3);
    EXPECT_EQ(pool.size(), 3u);
}

// Submit string result from lambda
TEST(ThreadPoolTest, SubmitReturnsString) {
    ThreadPool pool(2);
    auto fut = pool.submit([]() -> std::string { return "hello"; });
    EXPECT_EQ(fut.get(), "hello");
}

// Post multiple tasks accumulate correctly
TEST(ThreadPoolTest, PostFiveTasksAccumulate) {
    ThreadPool pool(2);
    std::atomic<int> total{0};
    for (int i = 1; i <= 5; ++i) {
        int val = i;
        pool.post([&total, val]() { total.fetch_add(val); });
    }
    pool.shutdown();
    EXPECT_EQ(total.load(), 15);
}

// Submit: future value is available after get()
TEST(ThreadPoolTest, FutureValueAvailableAfterGet) {
    ThreadPool pool(2);
    auto fut = pool.submit([]() { return 99; });
    int result = fut.get();
    EXPECT_EQ(result, 99);
}

// Submit bool-returning task (false value)
TEST(ThreadPoolTest, SubmitReturningBoolFalse) {
    ThreadPool pool(2);
    auto fut = pool.submit([]() -> bool { return false; });
    EXPECT_FALSE(fut.get());
}

// Post task that captures vector
TEST(ThreadPoolTest, PostTaskCapturesVector) {
    ThreadPool pool(2);
    std::vector<int> data = {10, 20, 30};
    std::atomic<int> sum{0};
    pool.post([data, &sum]() {
        int s = 0;
        for (int x : data) s += x;
        sum.store(s);
    });
    pool.shutdown();
    EXPECT_EQ(sum.load(), 60);
}

// ============================================================================
// Cycle 581: More ThreadPool tests
// ============================================================================

// Post task and verify it ran via promise
TEST(ThreadPoolTest, PostTaskViaPomise) {
    ThreadPool pool(2);
    std::promise<int> prom;
    auto fut = prom.get_future();
    pool.post([&prom]() { prom.set_value(123); });
    EXPECT_EQ(fut.get(), 123);
}

// Submit two tasks in parallel
TEST(ThreadPoolTest, TwoParallelSubmits) {
    ThreadPool pool(4);
    auto f1 = pool.submit([]() { return 10; });
    auto f2 = pool.submit([]() { return 20; });
    EXPECT_EQ(f1.get() + f2.get(), 30);
}

// Submit task that captures by value returns correct result
TEST(ThreadPoolTest, SubmitCaptureMath) {
    ThreadPool pool(2);
    int a = 7, b = 5;
    auto fut = pool.submit([a, b]() { return a * b; });
    EXPECT_EQ(fut.get(), 35);
}

// Pool size: hardware_concurrency is positive
TEST(ThreadPoolTest, DefaultSizeIsPositive) {
    ThreadPool pool;
    EXPECT_GT(pool.size(), 0u);
}

// Submit long: future<long long>
TEST(ThreadPoolTest, SubmitLongLong) {
    ThreadPool pool(2);
    auto fut = pool.submit([]() -> long long { return 9999999999LL; });
    EXPECT_EQ(fut.get(), 9999999999LL);
}

// Submit: zero value returned
TEST(ThreadPoolTest, SubmitReturnsZeroValue) {
    ThreadPool pool(2);
    auto fut = pool.submit([]() { return 0; });
    EXPECT_EQ(fut.get(), 0);
}

// Pool is running after construction
TEST(ThreadPoolTest, PoolIsRunningAfterConstruct) {
    ThreadPool pool(4);
    EXPECT_TRUE(pool.is_running());
    EXPECT_EQ(pool.size(), 4u);
}

// Shutdown does not throw
TEST(ThreadPoolTest, ShutdownDoesNotThrow) {
    ThreadPool pool(2);
    EXPECT_NO_THROW(pool.shutdown());
}

// ============================================================================
// Cycle 604: More ThreadPool tests
// ============================================================================

// Submit: capture string and return its length
TEST(ThreadPoolTest, SubmitCaptureStringLength) {
    ThreadPool pool(2);
    std::string str = "hello";
    auto fut = pool.submit([str]() { return (int)str.size(); });
    EXPECT_EQ(fut.get(), 5);
}

// Submit: double value returned
TEST(ThreadPoolTest, SubmitReturnsDouble) {
    ThreadPool pool(2);
    auto fut = pool.submit([]() -> double { return 3.14; });
    EXPECT_DOUBLE_EQ(fut.get(), 3.14);
}

// Post: ten tasks accumulate
TEST(ThreadPoolTest, PostTenTasksAccumulate) {
    ThreadPool pool(4);
    std::atomic<int> count{0};
    for (int i = 0; i < 10; ++i) {
        pool.post([&count]() { count++; });
    }
    pool.shutdown();
    EXPECT_EQ(count.load(), 10);
}

// Submit: negative int returned
TEST(ThreadPoolTest, SubmitReturnsNegativeInt) {
    ThreadPool pool(2);
    auto fut = pool.submit([]() { return -42; });
    EXPECT_EQ(fut.get(), -42);
}

// Pool size: five-thread pool reports five
TEST(ThreadPoolTest, FiveThreadPoolReportsFive) {
    ThreadPool pool(5);
    EXPECT_EQ(pool.size(), 5u);
}

// Submit: bool true returned
TEST(ThreadPoolTest, SubmitReturnsBoolTrue) {
    ThreadPool pool(2);
    auto fut = pool.submit([]() -> bool { return true; });
    EXPECT_TRUE(fut.get());
}

// Submit: sequential execution order via atomic
TEST(ThreadPoolTest, SubmitThreeSequentialIncrements) {
    ThreadPool pool(1);
    std::atomic<int> val{0};
    auto f1 = pool.submit([&val]() { val++; return val.load(); });
    auto f2 = pool.submit([&val]() { val++; return val.load(); });
    auto f3 = pool.submit([&val]() { val++; return val.load(); });
    f1.get(); f2.get(); f3.get();
    EXPECT_EQ(val.load(), 3);
}

// Submit: vector element sum
TEST(ThreadPoolTest, SubmitVectorSum) {
    ThreadPool pool(2);
    std::vector<int> v = {1, 2, 3, 4, 5};
    auto fut = pool.submit([v]() {
        int sum = 0;
        for (int x : v) sum += x;
        return sum;
    });
    EXPECT_EQ(fut.get(), 15);
}
