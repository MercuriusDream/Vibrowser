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
#include <array>

namespace clever::platform {

class EventLoop {
public:
    using Task = std::function<void()>;
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    enum class TaskSource : uint8_t {
        kInput = 0,
        kNetwork = 1,
        kTimer = 2,
        kRender = 3,
        kIdle = 4,
        kCount = 5,
    };

    EventLoop();
    ~EventLoop();

    // Non-copyable, non-movable
    EventLoop(const EventLoop&) = delete;
    EventLoop& operator=(const EventLoop&) = delete;

    // Post a task to be executed on the event loop thread
    void post_task(Task task);
    void post_task(Task task, TaskSource source);

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

    // Get the number of delayed tasks observed running later than their scheduled time.
    size_t delayed_task_starvation_count() const;

    // Get the duration this loop spent waiting for the last delayed-task wake.
    std::chrono::nanoseconds last_delayed_wait_duration() const;

    // Check whether run() is currently blocked on the next delayed-task deadline.
    bool is_waiting_on_delayed_task() const;

    // Get the number of times an earlier delayed task interrupted an armed wait_until().
    size_t earlier_deadline_wake_count() const;

private:
    struct DelayedTask {
        TimePoint run_at;
        uint64_t sequence = 0;
        Task task;
        bool operator>(const DelayedTask& other) const {
            if (run_at != other.run_at) {
                return run_at > other.run_at;
            }
            return sequence > other.sequence;
        }
    };

    std::array<std::deque<Task>, static_cast<size_t>(TaskSource::kCount)> tasks_by_source_;
    std::priority_queue<DelayedTask, std::vector<DelayedTask>, std::greater<>> delayed_tasks_;
    void move_ready_delayed_tasks_locked(TimePoint now);
    bool has_immediate_task_locked() const;
    Task pop_next_immediate_task_locked();
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    uint64_t next_delayed_task_sequence_ = 0;
    bool waiting_for_work_ = false;
    bool waiting_for_delayed_task_ = false;
    bool delayed_wait_rearm_pending_ = false;
    TimePoint delayed_wait_deadline_{};
    std::atomic<int64_t> last_delayed_task_lag_ns_{0};
    std::atomic<size_t> delayed_task_starvation_count_{0};
    std::atomic<int64_t> last_delayed_wait_duration_ns_{0};
    std::atomic<size_t> earlier_deadline_wake_count_{0};
    std::atomic<bool> running_{false};
    std::atomic<bool> quit_requested_{false};
};

} // namespace clever::platform
