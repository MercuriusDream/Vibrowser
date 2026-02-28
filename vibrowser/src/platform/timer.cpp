#include <clever/platform/timer.h>
#include <clever/platform/event_loop.h>

#include <atomic>
#include <memory>

namespace clever::platform {

struct Timer::Impl {
    EventLoop& loop;
    std::chrono::milliseconds interval;
    Callback callback;
    std::shared_ptr<std::atomic<bool>> cancelled;
    bool repeating;

    Impl(EventLoop& loop_, std::chrono::milliseconds interval_,
         Callback callback_, bool repeating_)
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
            interval
        );
    }

    static void schedule_repeating(EventLoop& loop, std::chrono::milliseconds interval,
                                   Callback callback,
                                   std::shared_ptr<std::atomic<bool>> cancelled) {
        loop.post_delayed_task(
            [&loop, interval, callback = std::move(callback),
             cancelled = std::move(cancelled)]() {
                if (cancelled->load()) {
                    return;
                }
                callback();
                if (!cancelled->load()) {
                    schedule_repeating(loop, interval, callback, cancelled);
                }
            },
            interval
        );
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
    Impl::schedule_repeating(loop, interval, timer->impl_->callback, cancelled);
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
