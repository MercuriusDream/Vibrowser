#include <clever/platform/event_loop.h>

namespace clever::platform {

EventLoop::EventLoop() = default;

EventLoop::~EventLoop() {
    quit();
}

void EventLoop::post_task(Task task) {
    {
        std::lock_guard lock(mutex_);
        tasks_.emplace_back(std::move(task));
    }
    cv_.notify_one();
}

void EventLoop::post_delayed_task(Task task, std::chrono::milliseconds delay) {
    const auto run_at = Clock::now() + delay;
    {
        std::lock_guard lock(mutex_);
        delayed_tasks_.push(DelayedTask{
            run_at,
            next_delayed_task_sequence_++,
            std::move(task)
        });
    }
    cv_.notify_one();
}

void EventLoop::move_ready_delayed_tasks_locked(TimePoint now, std::deque<Task>* ready_tasks) {
    while (!delayed_tasks_.empty() && delayed_tasks_.top().run_at <= now) {
        auto& delayed_task = const_cast<DelayedTask&>(delayed_tasks_.top());
        const auto lag = std::chrono::duration_cast<std::chrono::nanoseconds>(now - delayed_task.run_at);
        last_delayed_task_lag_ns_.store(lag.count(), std::memory_order_relaxed);

        if (ready_tasks != nullptr) {
            ready_tasks->emplace_back(std::move(delayed_task.task));
        } else {
            tasks_.emplace_back(std::move(delayed_task.task));
        }
        delayed_tasks_.pop();
    }
}

void EventLoop::run() {
    running_.store(true);
    quit_requested_.store(false);

    while (!quit_requested_.load()) {
        std::unique_lock lock(mutex_);

        move_ready_delayed_tasks_locked(Clock::now());

        if (!tasks_.empty()) {
            // Grab the next immediate task
            auto task = std::move(tasks_.front());
            tasks_.pop_front();
            lock.unlock();
            task();
            continue;
        }

        // No immediate tasks. Wait for either:
        // - A new task to be posted (cv_ notification)
        // - The next delayed task to become ready
        // - quit_requested_
        if (!delayed_tasks_.empty()) {
            auto next_time = delayed_tasks_.top().run_at;
            cv_.wait_until(lock, next_time, [this, next_time]() {
                return !tasks_.empty() || quit_requested_.load()
                    || delayed_tasks_.empty()
                    || delayed_tasks_.top().run_at != next_time;
            });
        } else {
            cv_.wait(lock, [this]() {
                return !tasks_.empty() || quit_requested_.load();
            });
        }
    }

    running_.store(false);
}

void EventLoop::run_pending() {
    // Move ready delayed tasks into the immediate queue, then run all
    // immediate tasks that are currently pending. Do NOT block.
    std::deque<Task> tasks_to_run;
    {
        std::lock_guard lock(mutex_);
        move_ready_delayed_tasks_locked(Clock::now());
        tasks_to_run.swap(tasks_);
    }

    for (auto& task : tasks_to_run) {
        task();
    }
}

void EventLoop::quit() {
    quit_requested_.store(true);
    cv_.notify_all();
}

bool EventLoop::is_running() const {
    return running_.load();
}

size_t EventLoop::pending_count() const {
    std::lock_guard lock(mutex_);
    return tasks_.size() + delayed_tasks_.size();
}

std::chrono::nanoseconds EventLoop::last_delayed_task_lag() const {
    return std::chrono::nanoseconds(last_delayed_task_lag_ns_.load(std::memory_order_relaxed));
}

} // namespace clever::platform
