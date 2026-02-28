#pragma once
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>

namespace clever::platform {

class ThreadPool {
public:
    explicit ThreadPool(size_t num_threads = std::thread::hardware_concurrency());
    ~ThreadPool();

    // Non-copyable, non-movable
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    // Submit a task and get a future for the result
    template<typename F, typename... Args>
    auto submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>>;

    // Submit a fire-and-forget task
    void post(std::function<void()> task);

    // Get number of worker threads
    size_t size() const;

    // Shutdown the pool (waits for pending tasks to complete)
    void shutdown();

    // Check if the pool is running
    bool is_running() const;

private:
    void worker_loop();

    std::vector<std::jthread> workers_;
    std::deque<std::function<void()>> tasks_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::atomic<bool> shutdown_{false};
};

// Template implementation
template<typename F, typename... Args>
auto ThreadPool::submit(F&& f, Args&&... args) -> std::future<std::invoke_result_t<F, Args...>> {
    using ReturnType = std::invoke_result_t<F, Args...>;
    auto task = std::make_shared<std::packaged_task<ReturnType()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    auto future = task->get_future();
    {
        std::lock_guard lock(mutex_);
        if (shutdown_) {
            throw std::runtime_error("ThreadPool is shut down");
        }
        tasks_.emplace_back([task]() { (*task)(); });
    }
    cv_.notify_one();
    return future;
}

} // namespace clever::platform
