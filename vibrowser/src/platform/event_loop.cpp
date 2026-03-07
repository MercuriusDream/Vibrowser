#include <clever/platform/event_loop.h>

namespace clever::platform {
namespace {

constexpr auto kTaskSourcePriorityOrder = std::to_array<EventLoop::TaskSource>({
    EventLoop::TaskSource::kInput,
    EventLoop::TaskSource::kNetwork,
    EventLoop::TaskSource::kTimer,
    EventLoop::TaskSource::kRender,
    EventLoop::TaskSource::kIdle,
});

} // namespace

EventLoop::EventLoop() = default;

EventLoop::~EventLoop() {
    quit();
}

void EventLoop::post_task(Task task) {
    post_task(std::move(task), TaskSource::kInput);
}

void EventLoop::post_task(Task task, TaskSource source) {
    {
        std::lock_guard lock(mutex_);
        tasks_by_source_[static_cast<size_t>(source)].emplace_back(std::move(task));
    }
    cv_.notify_one();
}

bool EventLoop::has_immediate_task_locked() const {
    for (const auto source : kTaskSourcePriorityOrder) {
        const auto& tasks = tasks_by_source_[static_cast<size_t>(source)];
        if (!tasks.empty()) {
            return true;
        }
    }
    return false;
}

EventLoop::Task EventLoop::pop_next_immediate_task_locked() {
    for (const auto source : kTaskSourcePriorityOrder) {
        auto& tasks = tasks_by_source_[static_cast<size_t>(source)];
        if (!tasks.empty()) {
            auto task = std::move(tasks.front());
            tasks.pop_front();
            return task;
        }
    }
    return {};
}

void EventLoop::post_delayed_task(Task task, std::chrono::milliseconds delay) {
    const auto run_at = Clock::now() + delay;
    bool should_wake_runner = false;
    {
        std::lock_guard lock(mutex_);
        delayed_tasks_.push(DelayedTask{
            run_at,
            next_delayed_task_sequence_++,
            std::move(task)
        });
        if (waiting_for_work_) {
            should_wake_runner = true;
        } else if (waiting_for_delayed_task_ && !delayed_wait_rearm_pending_
            && run_at < delayed_wait_deadline_) {
            // Wake run() so it can stop waiting on the stale later deadline and
            // re-arm against the new earliest delayed task immediately.
            delayed_wait_rearm_pending_ = true;
            earlier_deadline_wake_count_.fetch_add(1, std::memory_order_relaxed);
            should_wake_runner = true;
        }
    }
    if (should_wake_runner) {
        cv_.notify_one();
    }
}

void EventLoop::move_ready_delayed_tasks_locked(TimePoint now) {
    while (!delayed_tasks_.empty() && delayed_tasks_.top().run_at <= now) {
        auto& delayed_task = const_cast<DelayedTask&>(delayed_tasks_.top());
        const auto lag = std::chrono::duration_cast<std::chrono::nanoseconds>(now - delayed_task.run_at);
        if (lag.count() > 0) {
            delayed_task_starvation_count_.fetch_add(1, std::memory_order_relaxed);
        }
        last_delayed_task_lag_ns_.store(lag.count(), std::memory_order_relaxed);

        // Always promote delayed work into the timer source queue so both
        // run() and run_pending() preserve source priority after wake-up.
        tasks_by_source_[static_cast<size_t>(TaskSource::kTimer)].emplace_back(
            std::move(delayed_task.task));
        delayed_tasks_.pop();
    }
}

void EventLoop::run() {
    running_.store(true);
    quit_requested_.store(false);

    while (!quit_requested_.load()) {
        std::unique_lock lock(mutex_);

        move_ready_delayed_tasks_locked(Clock::now());

        if (has_immediate_task_locked()) {
            // Immediate work is popped strictly by TaskSource order, so nested
            // posts and promoted zero-delay timers are resolved deterministically.
            auto task = pop_next_immediate_task_locked();
            lock.unlock();
            task();
            continue;
        }

        // No immediate tasks. Wait for either:
        // - A new task to be posted (cv_ notification)
        // - The next delayed task to become ready
        // - quit_requested_
        if (!delayed_tasks_.empty()) {
            delayed_wait_deadline_ = delayed_tasks_.top().run_at;
            waiting_for_delayed_task_ = true;
            delayed_wait_rearm_pending_ = false;
            const auto block_start = Clock::now();
            cv_.wait_until(lock, delayed_wait_deadline_, [this]() {
                return has_immediate_task_locked() || quit_requested_.load()
                    || delayed_tasks_.empty()
                    || delayed_wait_rearm_pending_;
            });
            waiting_for_delayed_task_ = false;
            delayed_wait_rearm_pending_ = false;
            delayed_wait_deadline_ = TimePoint{};
            const auto blocked_duration =
                std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - block_start);
            last_delayed_wait_duration_ns_.store(blocked_duration.count(), std::memory_order_relaxed);
        } else {
            waiting_for_work_ = true;
            cv_.wait(lock, [this]() {
                return has_immediate_task_locked() || !delayed_tasks_.empty() || quit_requested_.load();
            });
            waiting_for_work_ = false;
        }
    }

    running_.store(false);
}

void EventLoop::run_pending() {
    // Keep running while immediate work remains to mirror run() scheduling
    // behavior, including re-entrant posts and delayed-task promotion.
    while (true) {
        Task task_to_run;
        {
            std::lock_guard lock(mutex_);
            move_ready_delayed_tasks_locked(Clock::now());
            if (!has_immediate_task_locked()) {
                break;
            }
            task_to_run = pop_next_immediate_task_locked();
        }

        task_to_run();
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
    size_t pending = delayed_tasks_.size();
    for (const auto& tasks : tasks_by_source_) {
        pending += tasks.size();
    }
    return pending;
}

std::chrono::nanoseconds EventLoop::last_delayed_task_lag() const {
    return std::chrono::nanoseconds(last_delayed_task_lag_ns_.load(std::memory_order_relaxed));
}

size_t EventLoop::delayed_task_starvation_count() const {
    return delayed_task_starvation_count_.load(std::memory_order_relaxed);
}

std::chrono::nanoseconds EventLoop::last_delayed_wait_duration() const {
    return std::chrono::nanoseconds(last_delayed_wait_duration_ns_.load(std::memory_order_relaxed));
}

bool EventLoop::is_waiting_on_delayed_task() const {
    std::lock_guard lock(mutex_);
    return waiting_for_delayed_task_;
}

size_t EventLoop::earlier_deadline_wake_count() const {
    return earlier_deadline_wake_count_.load(std::memory_order_relaxed);
}

} // namespace clever::platform
