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
    {
        std::lock_guard lock(mutex_);
        delayed_tasks_.push(DelayedTask{
            Clock::now() + delay,
            std::move(task)
        });
    }
    cv_.notify_one();
}

void EventLoop::run() {
    running_.store(true);
    quit_requested_.store(false);

    while (!quit_requested_.load()) {
        std::unique_lock lock(mutex_);

        // Move any delayed tasks that are ready into the immediate queue
        auto now = Clock::now();
        while (!delayed_tasks_.empty() && delayed_tasks_.top().run_at <= now) {
            // priority_queue only provides const& top(), so we need a const_cast
            // or copy. We copy the task out then pop.
            tasks_.emplace_back(std::move(const_cast<DelayedTask&>(delayed_tasks_.top()).task));
            delayed_tasks_.pop();
        }

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
            cv_.wait_until(lock, next_time, [this]() {
                return !tasks_.empty() || quit_requested_.load();
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
        auto now = Clock::now();
        while (!delayed_tasks_.empty() && delayed_tasks_.top().run_at <= now) {
            tasks_.emplace_back(std::move(const_cast<DelayedTask&>(delayed_tasks_.top()).task));
            delayed_tasks_.pop();
        }
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

} // namespace clever::platform
