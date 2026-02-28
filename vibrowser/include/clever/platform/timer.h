#pragma once
#include <chrono>
#include <functional>
#include <memory>

namespace clever::platform {

class EventLoop;

class Timer {
public:
    using Callback = std::function<void()>;

    // One-shot timer
    static std::unique_ptr<Timer> one_shot(
        EventLoop& loop,
        std::chrono::milliseconds delay,
        Callback callback);

    // Repeating timer
    static std::unique_ptr<Timer> repeating(
        EventLoop& loop,
        std::chrono::milliseconds interval,
        Callback callback);

    ~Timer();

    void cancel();
    bool is_active() const;

private:
    Timer() = default;
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace clever::platform
