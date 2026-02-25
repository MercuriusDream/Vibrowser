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
