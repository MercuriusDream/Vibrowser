#include <clever/platform/thread_pool.h>

#include <stdexcept>

namespace clever::platform {

ThreadPool::ThreadPool(size_t num_threads) {
    workers_.reserve(num_threads);
    for (size_t i = 0; i < num_threads; ++i) {
        workers_.emplace_back([this]() { worker_loop(); });
    }
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::post(std::function<void()> task) {
    {
        std::lock_guard lock(mutex_);
        if (shutdown_) {
            throw std::runtime_error("ThreadPool is shut down");
        }
        tasks_.emplace_back(std::move(task));
    }
    cv_.notify_one();
}

size_t ThreadPool::size() const {
    return workers_.size();
}

void ThreadPool::shutdown() {
    {
        std::lock_guard lock(mutex_);
        if (shutdown_) {
            return; // Already shut down
        }
        shutdown_ = true;
    }
    cv_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

bool ThreadPool::is_running() const {
    return !shutdown_.load();
}

void ThreadPool::worker_loop() {
    while (true) {
        std::function<void()> task;
        {
            std::unique_lock lock(mutex_);
            cv_.wait(lock, [this]() {
                return shutdown_.load() || !tasks_.empty();
            });

            if (tasks_.empty()) {
                // shutdown_ is true and no more tasks
                return;
            }

            task = std::move(tasks_.front());
            tasks_.pop_front();
        }
        task();
    }
}

} // namespace clever::platform
