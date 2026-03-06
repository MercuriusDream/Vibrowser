#pragma once
#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <queue>
#include <unordered_map>
#include <vector>

namespace clever::platform {

class EventLoop;

class TimerScheduler {
public:
    using Callback = std::function<void()>;
    using Duration = std::chrono::milliseconds;
    using TimePoint = Duration;

    TimerScheduler() = default;

    int schedule_timeout(Duration delay, Callback callback);
    int schedule_interval(Duration interval, Callback callback);
    bool cancel(int id);
    bool is_active(int id) const;
    size_t run_ready(Duration advance = Duration::zero());
    void clear();

private:
    struct ScheduledTask;
    struct QueueEntry {
        TimePoint due_time { Duration::zero() };
        uint64_t sequence = 0;
        int id = 0;
    };
    struct QueueEntryGreater {
        bool operator()(const QueueEntry& lhs, const QueueEntry& rhs) const;
    };

    int schedule(Duration delay, Duration interval, bool repeating, Callback callback);

    TimePoint now_ { Duration::zero() };
    int next_id_ { 1 };
    uint64_t next_sequence_ { 1 };
    std::priority_queue<QueueEntry, std::vector<QueueEntry>, QueueEntryGreater> queue_;
    std::unordered_map<int, std::shared_ptr<ScheduledTask>> tasks_;
};

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
