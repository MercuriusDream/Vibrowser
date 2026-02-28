#pragma once

#include <memory>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <functional>

extern "C" {
struct JSContext;
struct JSRuntime;
}

namespace clever::js {

// Serialized message to pass between worker and main thread
struct WorkerMessage {
    std::string data;  // JSON-serialized message data
};

// Internal worker thread state (opaque to JS)
class WorkerThread {
public:
    explicit WorkerThread(const std::string& script_url);
    ~WorkerThread();

    // Non-copyable, non-movable
    WorkerThread(const WorkerThread&) = delete;
    WorkerThread& operator=(const WorkerThread&) = delete;

    // Start the worker thread
    void start(std::function<std::string(const std::string&)> module_fetcher);

    // Send a message from main thread to worker
    void post_message_to_worker(const std::string& json_data);

    // Try to receive a message from worker to main thread (non-blocking)
    // Returns empty string if no message available
    std::string try_recv_message_from_worker();

    // Terminate the worker thread (async)
    void terminate();

    // Check if worker is still running
    bool is_running() const;

    // Check if worker thread has finished (for cleanup)
    bool is_finished() const { return finished_; }

private:
    void worker_main();

    std::string script_url_;
    JSRuntime* worker_rt_ = nullptr;
    JSContext* worker_ctx_ = nullptr;
    std::thread worker_thread_;

    // Message queues
    std::queue<WorkerMessage> main_to_worker_;
    std::queue<WorkerMessage> worker_to_main_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;

    bool should_terminate_ = false;
    bool finished_ = false;
    std::function<std::string(const std::string&)> module_fetcher_;
};

// Install Worker constructor and related classes into a JS context
void install_worker_bindings(JSContext* ctx);

// Process any pending worker messages in the main thread
// Call this periodically to receive messages from active workers
void process_worker_messages(JSContext* ctx);

} // namespace clever::js
