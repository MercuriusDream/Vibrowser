#pragma once
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <queue>
#include <vector>

namespace clever::platform {

class EventLoop {
public:
    using Task = std::function<void()>;
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    EventLoop();
    ~EventLoop();

    // Non-copyable, non-movable
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    // Post a task to be executed on the event loop thread
    void post_task(Task task);

    // Post a task to be executed after a delay
    void post_delayed_task(Task task, std::chrono::milliseconds delay);

    // Run the event loop (blocks until quit() is called)
    void run();

    // Run pending tasks and return (non-blocking)
    void run_pending();

    // Request the event loop to stop
    void quit();

    // Check if the event loop is running
    bool is_running() const;

    // Get the number of pending tasks
    size_t pending_count() const;

    // Get the lag observed when the most recently promoted delayed task woke late.
    std::chrono::nanoseconds last_delayed_task_lag() const;

private:
    struct DelayedTask {
        TimePoint run_at;
        Task task;
        bool operator>(const DelayedTask& other) const {
            return run_at > other.run_at;
        }
    };

    std::deque<Task> tasks_;
    std::priority_queue<DelayedTask, std::vector<DelayedTask>, std::greater<>> delayed_tasks_;
    void move_ready_delayed_tasks_locked(TimePoint now, std::deque<Task>* ready_tasks = nullptr);
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    size_t delayed_queue_generation_ = 0;
    std::atomic<int64_t> last_delayed_task_lag_ns_{0};
    std::atomic<bool> running_{false};
    std::atomic<bool> quit_requested_{false};
};

} // namespace clever::platform
