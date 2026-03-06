#include <clever/platform/timer.h>
#include <clever/platform/event_loop.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <utility>
#include <vector>

namespace clever::platform {

struct TimerScheduler::ScheduledTask {
    int id = 0;
    TimePoint due_time { Duration::zero() };
    Duration interval { Duration::zero() };
    Callback callback;
    bool repeating = false;
    bool cancelled = false;
    uint64_t sequence = 0;
};

bool TimerScheduler::QueueEntryGreater::operator()(const QueueEntry& lhs, const QueueEntry& rhs) const {
    if (lhs.due_time != rhs.due_time) {
        return lhs.due_time > rhs.due_time;
    }
    return lhs.sequence > rhs.sequence;
}

int TimerScheduler::schedule_timeout(Duration delay, Callback callback) {
    return schedule(std::max(delay, Duration::zero()), Duration::zero(), false, std::move(callback));
}

int TimerScheduler::schedule_interval(Duration interval, Callback callback) {
    return schedule(std::max(interval, Duration::zero()), std::max(interval, Duration::zero()), true,
        std::move(callback));
}

bool TimerScheduler::cancel(int id) {
    auto it = tasks_.find(id);
    if (it == tasks_.end()) {
        return false;
    }

    it->second->cancelled = true;
    tasks_.erase(it);
    return true;
}

bool TimerScheduler::is_active(int id) const {
    auto it = tasks_.find(id);
    return it != tasks_.end() && !it->second->cancelled;
}

size_t TimerScheduler::run_ready(Duration advance) {
    if (advance < Duration::zero()) {
        advance = Duration::zero();
    }
    now_ += advance;

    std::vector<std::shared_ptr<ScheduledTask>> ready_tasks;
    while (!queue_.empty()) {
        auto entry = queue_.top();
        if (entry.due_time > now_) {
            break;
        }
        queue_.pop();

        auto it = tasks_.find(entry.id);
        if (it == tasks_.end()) {
            continue;
        }

        auto task = it->second;
        if (task->cancelled || task->sequence != entry.sequence || task->due_time != entry.due_time) {
            continue;
        }

        ready_tasks.push_back(std::move(task));
    }

    size_t fired = 0;
    for (auto& task : ready_tasks) {
        if (task->cancelled) {
            continue;
        }

        task->callback();
        fired++;

        auto it = tasks_.find(task->id);
        if (!task->repeating || task->cancelled || it == tasks_.end()) {
            tasks_.erase(task->id);
            continue;
        }

        if (task->interval <= Duration::zero()) {
            task->due_time = now_;
        } else {
            do {
                task->due_time += task->interval;
            } while (task->due_time <= now_);
        }
        task->sequence = next_sequence_++;
        queue_.push(QueueEntry { task->due_time, task->sequence, task->id });
    }

    return fired;
}

void TimerScheduler::clear() {
    tasks_.clear();
    queue_ = {};
}

int TimerScheduler::schedule(Duration delay, Duration interval, bool repeating, Callback callback) {
    auto task = std::make_shared<ScheduledTask>();
    task->id = next_id_++;
    task->due_time = now_ + std::max(delay, Duration::zero());
    task->interval = interval;
    task->callback = std::move(callback);
    task->repeating = repeating;
    task->sequence = next_sequence_++;

    tasks_[task->id] = task;
    queue_.push(QueueEntry { task->due_time, task->sequence, task->id });
    return task->id;
}

struct Timer::Impl {
    EventLoop& loop;
    std::chrono::milliseconds interval;
    Callback callback;
    std::shared_ptr<std::atomic<bool>> cancelled;
    bool repeating;

    Impl(EventLoop& loop_, std::chrono::milliseconds interval_, Callback callback_, bool repeating_)
        : loop(loop_)
        , interval(interval_)
        , callback(std::move(callback_))
        , cancelled(std::make_shared<std::atomic<bool>>(false))
        , repeating(repeating_) {}

    void schedule_one_shot() {
        auto cb = callback;
        auto cancelled_flag = cancelled;

        loop.post_delayed_task(
            [cb, cancelled_flag]() {
                if (!cancelled_flag->load()) {
                    cb();
                }
            },
            interval);
    }

    static void schedule_repeating(EventLoop& loop, std::chrono::milliseconds interval,
        std::chrono::steady_clock::time_point expected_run_at, Callback callback,
        std::shared_ptr<std::atomic<bool>> cancelled) {
        auto initial_delay = std::chrono::duration_cast<std::chrono::milliseconds>(
            expected_run_at - std::chrono::steady_clock::now());
        if (initial_delay < std::chrono::milliseconds::zero()) {
            initial_delay = std::chrono::milliseconds::zero();
        }

        loop.post_delayed_task(
            [&loop, interval, expected_run_at, callback = std::move(callback),
                cancelled = std::move(cancelled)]() mutable {
                if (cancelled->load()) {
                    return;
                }

                callback();
                if (cancelled->load()) {
                    return;
                }

                auto next_run_at = expected_run_at;
                if (interval <= std::chrono::milliseconds::zero()) {
                    next_run_at = std::chrono::steady_clock::now();
                } else {
                    do {
                        next_run_at += interval;
                    } while (next_run_at <= std::chrono::steady_clock::now());
                }

                auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(
                    next_run_at - std::chrono::steady_clock::now());
                if (delay < std::chrono::milliseconds::zero()) {
                    delay = std::chrono::milliseconds::zero();
                }

                schedule_repeating(loop, interval, next_run_at, std::move(callback), std::move(cancelled));
            },
            initial_delay);
    }
};

std::unique_ptr<Timer> Timer::one_shot(
    EventLoop& loop,
    std::chrono::milliseconds delay,
    Callback callback)
{
    auto timer = std::unique_ptr<Timer>(new Timer());
    timer->impl_ = std::make_unique<Impl>(loop, delay, std::move(callback), false);
    timer->impl_->schedule_one_shot();
    return timer;
}

std::unique_ptr<Timer> Timer::repeating(
    EventLoop& loop,
    std::chrono::milliseconds interval,
    Callback callback)
{
    auto timer = std::unique_ptr<Timer>(new Timer());
    timer->impl_ = std::make_unique<Impl>(loop, interval, std::move(callback), true);

    auto cancelled = timer->impl_->cancelled;
    auto first_run_at = std::chrono::steady_clock::now() + interval;
    Impl::schedule_repeating(loop, interval, first_run_at, timer->impl_->callback, cancelled);
    return timer;
}

Timer::~Timer() {
    cancel();
}

void Timer::cancel() {
    if (impl_ && impl_->cancelled) {
        impl_->cancelled->store(true);
    }
}

bool Timer::is_active() const {
    return impl_ && impl_->cancelled && !impl_->cancelled->load();
}

} // namespace clever::platform
