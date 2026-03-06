#include <clever/platform/timer.h>
#include <clever/platform/event_loop.h>

#include <algorithm>
#include <atomic>
#include <mutex>
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
    struct OneShotState {
        Callback callback;
        mutable std::mutex mutex;
        std::atomic<bool> active { true };

        explicit OneShotState(Callback callback_)
            : callback(std::move(callback_)) {}
    };

    struct RepeatingState {
        EventLoop& loop;
        std::chrono::milliseconds interval;
        Callback callback;
        std::mutex mutex;
        bool cancelled = false;
        std::chrono::steady_clock::time_point next_run_at {};

        RepeatingState(EventLoop& loop_, std::chrono::milliseconds interval_, Callback callback_)
            : loop(loop_)
            , interval(interval_)
            , callback(std::move(callback_)) {}
    };

    EventLoop& loop;
    std::chrono::milliseconds interval;
    Callback callback;
    std::shared_ptr<OneShotState> one_shot_state;
    std::shared_ptr<RepeatingState> repeating_state;
    bool repeating;

    Impl(EventLoop& loop_, std::chrono::milliseconds interval_, Callback callback_, bool repeating_)
        : loop(loop_)
        , interval(interval_)
        , callback(std::move(callback_))
        , one_shot_state(repeating_ ? nullptr : std::make_shared<OneShotState>(callback))
        , repeating_state(repeating_ ? std::make_shared<RepeatingState>(loop_, interval_, callback) : nullptr)
        , repeating(repeating_) {}

    void schedule_one_shot() {
        auto state = one_shot_state;

        loop.post_delayed_task(
            [state]() {
                if (!state->active.exchange(false)) {
                    return;
                }

                Callback callback;
                {
                    std::lock_guard lock(state->mutex);
                    callback = std::move(state->callback);
                }

                if (!callback) {
                    return;
                }
                callback();
            },
            interval);
    }

    static void schedule_repeating(const std::shared_ptr<RepeatingState>& state,
        std::chrono::steady_clock::time_point expected_run_at) {
        auto delay = std::chrono::duration_cast<std::chrono::milliseconds>(
            expected_run_at - std::chrono::steady_clock::now());
        if (delay < std::chrono::milliseconds::zero()) {
            delay = std::chrono::milliseconds::zero();
        }

        {
            std::lock_guard lock(state->mutex);
            if (state->cancelled) {
                return;
            }
            state->next_run_at = expected_run_at;
            state->loop.post_delayed_task(
                [state, expected_run_at]() {
                    Callback callback;
                    std::chrono::milliseconds interval;
                    {
                        std::lock_guard lock(state->mutex);
                        if (state->cancelled || state->next_run_at != expected_run_at) {
                            return;
                        }
                        callback = state->callback;
                        interval = state->interval;
                    }

                    callback();

                    auto now = std::chrono::steady_clock::now();
                    auto next_run_at = expected_run_at;
                    if (interval <= std::chrono::milliseconds::zero()) {
                        next_run_at = now;
                    } else {
                        do {
                            next_run_at += interval;
                        } while (next_run_at <= now);
                    }

                    schedule_repeating(state, next_run_at);
                },
                delay);
        }
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

    auto first_run_at = std::chrono::steady_clock::now() + interval;
    Impl::schedule_repeating(timer->impl_->repeating_state, first_run_at);
    return timer;
}

Timer::~Timer() {
    cancel();
}

void Timer::cancel() {
    if (!impl_) {
        return;
    }

    if (impl_->repeating_state) {
        std::lock_guard lock(impl_->repeating_state->mutex);
        impl_->repeating_state->cancelled = true;
    }

    if (impl_->one_shot_state && impl_->one_shot_state->active.exchange(false)) {
        std::lock_guard lock(impl_->one_shot_state->mutex);
        impl_->one_shot_state->callback = {};
    }
}

bool Timer::is_active() const {
    if (!impl_) {
        return false;
    }
    if (impl_->repeating_state) {
        std::lock_guard lock(impl_->repeating_state->mutex);
        return !impl_->repeating_state->cancelled;
    }
    return impl_->one_shot_state && impl_->one_shot_state->active.load();
}

} // namespace clever::platform
