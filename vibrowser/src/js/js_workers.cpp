#include <clever/js/js_workers.h>

extern "C" {
#include <quickjs.h>
}

#include <sstream>
#include <chrono>
#include <unordered_map>

namespace clever::js {

namespace {

// =========================================================================
// Class IDs and per-context state
// =========================================================================

static JSClassID worker_class_id = 0;

struct WorkerStateEntry {
    std::shared_ptr<WorkerThread> thread;
    JSValue js_object;  // Keep reference to the JS object for message delivery
};

struct WorkerState {
    // Map of worker threads to their JS objects and state
    std::unordered_map<WorkerThread*, WorkerStateEntry> workers;
    JSContext* ctx = nullptr;
};

// Get or create WorkerState for a context
WorkerState* get_worker_state(JSContext* ctx) {
    WorkerState* state = static_cast<WorkerState*>(JS_GetContextOpaque(ctx));
    if (!state) {
        state = new WorkerState();
        state->ctx = ctx;
        JS_SetContextOpaque(ctx, state);
    }
    return state;
}

// =========================================================================
// Worker finalizer
// =========================================================================

static void worker_finalizer(JSRuntime* /*rt*/, JSValue val) {
    WorkerThread* worker = static_cast<WorkerThread*>(JS_GetOpaque(val, worker_class_id));
    if (worker) {
        worker->terminate();
    }
}

// =========================================================================
// Worker.postMessage(data)
// =========================================================================

static JSValue worker_post_message(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    WorkerThread* worker = static_cast<WorkerThread*>(JS_GetOpaque(this_val, worker_class_id));
    if (!worker) {
        return JS_ThrowTypeError(ctx, "Invalid worker object");
    }

    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "postMessage requires at least 1 argument");
    }

    // Serialize the data using JSON
    const char* json_str = JS_JSONStringify(ctx, argv[0], JS_NULL, JS_NULL);
    if (!json_str) {
        return JS_EXCEPTION;
    }

    std::string json_data(json_str);
    JS_FreeCString(ctx, json_str);

    // Post message to worker
    worker->post_message_to_worker(json_data);

    return JS_UNDEFINED;
}

// =========================================================================
// Worker.terminate()
// =========================================================================

static JSValue worker_terminate(JSContext* ctx, JSValueConst this_val, int /*argc*/, JSValueConst* /*argv*/) {
    WorkerThread* worker = static_cast<WorkerThread*>(JS_GetOpaque(this_val, worker_class_id));
    if (!worker) {
        return JS_ThrowTypeError(ctx, "Invalid worker object");
    }

    worker->terminate();
    return JS_UNDEFINED;
}

// =========================================================================
// Worker constructor
// =========================================================================

static JSValue worker_constructor(JSContext* ctx, JSValueConst /*new_target*/, int argc, JSValueConst* argv) {
    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "Worker requires a script URL");
    }

    const char* script_url = JS_ToCString(ctx, argv[0]);
    if (!script_url) {
        return JS_EXCEPTION;
    }

    std::string url(script_url);
    JS_FreeCString(ctx, script_url);

    // Create a new WorkerThread
    auto worker = std::make_shared<WorkerThread>(url);

    // Get module fetcher from context (placeholder for now)
    auto module_fetcher = [](const std::string& /*url*/) -> std::string {
        return "";
    };

    worker->start(module_fetcher);

    // Create the JS object for the worker
    JSValue worker_obj = JS_NewObjectClass(ctx, worker_class_id);
    if (JS_IsException(worker_obj)) {
        return JS_EXCEPTION;
    }

    WorkerThread* worker_ptr = worker.get();
    JS_SetOpaque(worker_obj, worker_ptr);

    // Store the worker in the context state
    WorkerState* state = get_worker_state(ctx);
    state->workers[worker_ptr] = WorkerStateEntry{worker, JS_DupValue(ctx, worker_obj)};

    // Add methods: postMessage, terminate
    JS_DefinePropertyValueStr(ctx, worker_obj, "postMessage",
                             JS_NewCFunction(ctx, worker_post_message, "postMessage", 1),
                             JS_PROP_CONFIGURABLE);

    JS_DefinePropertyValueStr(ctx, worker_obj, "terminate",
                             JS_NewCFunction(ctx, worker_terminate, "terminate", 0),
                             JS_PROP_CONFIGURABLE);

    // Add onmessage handler (initially null)
    JS_DefinePropertyValueStr(ctx, worker_obj, "onmessage", JS_NULL, JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);

    // Add onerror handler (initially null)
    JS_DefinePropertyValueStr(ctx, worker_obj, "onerror", JS_NULL, JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);

    return worker_obj;
}

// =========================================================================
// self.postMessage(data) in worker context
// =========================================================================

static JSValue worker_self_post_message(JSContext* ctx, JSValueConst this_val, int argc, JSValueConst* argv) {
    (void)this_val;

    if (argc < 1) {
        return JS_ThrowTypeError(ctx, "postMessage requires at least 1 argument");
    }

    // Serialize the data using JSON
    const char* json_str = JS_JSONStringify(ctx, argv[0], JS_NULL, JS_NULL);
    if (!json_str) {
        return JS_EXCEPTION;
    }

    std::string json_data(json_str);
    JS_FreeCString(ctx, json_str);

    // Get the WorkerThread from context opaque (if available)
    WorkerThread* worker = static_cast<WorkerThread*>(JS_GetContextOpaque(ctx));
    if (worker) {
        worker->post_message_to_worker(json_data);
    }

    return JS_UNDEFINED;
}

}  // anonymous namespace

// =========================================================================
// WorkerThread implementation
// =========================================================================

WorkerThread::WorkerThread(const std::string& script_url)
    : script_url_(script_url), worker_rt_(nullptr), worker_ctx_(nullptr) {
}

WorkerThread::~WorkerThread() {
    if (worker_ctx_) {
        JS_FreeContext(worker_ctx_);
    }
    if (worker_rt_) {
        JS_FreeRuntime(worker_rt_);
    }
}

void WorkerThread::start(std::function<std::string(const std::string&)> module_fetcher) {
    module_fetcher_ = module_fetcher;
    worker_thread_ = std::thread(&WorkerThread::worker_main, this);
}

void WorkerThread::post_message_to_worker(const std::string& json_data) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        main_to_worker_.push(WorkerMessage{json_data});
    }
    queue_cv_.notify_one();
}

std::string WorkerThread::try_recv_message_from_worker() {
    std::lock_guard<std::mutex> lock(queue_mutex_);
    if (worker_to_main_.empty()) {
        return "";
    }
    std::string result = worker_to_main_.front().data;
    worker_to_main_.pop();
    return result;
}

void WorkerThread::terminate() {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        should_terminate_ = true;
    }
    queue_cv_.notify_one();

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

bool WorkerThread::is_running() const {
    return !finished_;
}

void WorkerThread::worker_main() {
    // Create a new JS runtime and context for this worker
    worker_rt_ = JS_NewRuntime();
    if (!worker_rt_) {
        finished_ = true;
        return;
    }

    worker_ctx_ = JS_NewContext(worker_rt_);
    if (!worker_ctx_) {
        JS_FreeRuntime(worker_rt_);
        finished_ = true;
        return;
    }

    // Set context opaque to this worker thread for self.postMessage
    JS_SetContextOpaque(worker_ctx_, this);

    // Set up console.log for the worker
    JSValue console_obj = JS_NewObject(worker_ctx_);
    JSValue log_func = JS_NewCFunction(worker_ctx_,
        [](JSContext* ctx, JSValueConst /*this_val*/, int argc, JSValueConst* argv) -> JSValue {
            for (int i = 0; i < argc; i++) {
                const char* str = JS_ToCString(ctx, argv[i]);
                if (str) {
                    JS_FreeCString(ctx, str);
                }
            }
            return JS_UNDEFINED;
        }, "log", 0);
    JS_DefinePropertyValueStr(worker_ctx_, console_obj, "log", log_func, 0);
    JS_DefinePropertyValueStr(worker_ctx_, JS_GetGlobalObject(worker_ctx_), "console", console_obj, 0);
    JS_FreeValue(worker_ctx_, console_obj);

    // Set up self object with postMessage and onmessage
    JSValue self_obj = JS_GetGlobalObject(worker_ctx_);
    JS_DefinePropertyValueStr(worker_ctx_, self_obj, "postMessage",
                             JS_NewCFunction(worker_ctx_, worker_self_post_message, "postMessage", 1),
                             JS_PROP_CONFIGURABLE);
    JS_DefinePropertyValueStr(worker_ctx_, self_obj, "onmessage", JS_NULL, JS_PROP_CONFIGURABLE | JS_PROP_WRITABLE);

    // Fetch and evaluate the worker script
    std::string script_source;
    if (module_fetcher_) {
        script_source = module_fetcher_(script_url_);
    }

    if (!script_source.empty()) {
        JSValue result = JS_Eval(worker_ctx_, script_source.c_str(), script_source.size(),
                                script_url_.c_str(), JS_EVAL_TYPE_GLOBAL);
        if (JS_IsException(result)) {
            JSValue error = JS_GetException(worker_ctx_);
            const char* error_str = JS_ToCString(worker_ctx_, error);
            if (error_str) {
                JS_FreeCString(worker_ctx_, error_str);
            }
            JS_FreeValue(worker_ctx_, error);
        }
        JS_FreeValue(worker_ctx_, result);
    }

    // Message loop
    while (!should_terminate_) {
        std::unique_lock<std::mutex> lock(queue_mutex_);
        queue_cv_.wait_for(lock, std::chrono::milliseconds(100),
                          [this]() { return !main_to_worker_.empty() || should_terminate_; });

        if (should_terminate_) {
            break;
        }

        if (!main_to_worker_.empty()) {
            WorkerMessage msg = main_to_worker_.front();
            main_to_worker_.pop();
            lock.unlock();

            // Parse JSON and call onmessage handler
            JSValue json_val = JS_JSONParse(worker_ctx_, msg.data.c_str(), msg.data.c_str() + msg.data.size());
            if (!JS_IsException(json_val)) {
                JSValue event_obj = JS_NewObject(worker_ctx_);
                JS_DefinePropertyValueStr(worker_ctx_, event_obj, "data", json_val, 0);

                JSValue onmessage = JS_GetPropertyStr(worker_ctx_, self_obj, "onmessage");
                if (JS_IsFunction(worker_ctx_, onmessage)) {
                    JSValue call_result = JS_Call(worker_ctx_, onmessage, self_obj, 1, &event_obj);
                    if (JS_IsException(call_result)) {
                        JS_FreeValue(worker_ctx_, JS_GetException(worker_ctx_));
                    }
                    JS_FreeValue(worker_ctx_, call_result);
                }
                JS_FreeValue(worker_ctx_, onmessage);
                JS_FreeValue(worker_ctx_, event_obj);
            } else {
                JS_FreeValue(worker_ctx_, JS_GetException(worker_ctx_));
            }
        }
    }

    finished_ = true;
}

// =========================================================================
// Public API
// =========================================================================

void install_worker_bindings(JSContext* ctx) {
    JSValue global = JS_GetGlobalObject(ctx);

    // Create Worker class
    JSClassDef worker_class = {
        "Worker",
        .finalizer = worker_finalizer,
    };

    JS_NewClassID(&worker_class_id);
    JS_NewClass(JS_GetRuntime(ctx), worker_class_id, &worker_class);

    // Create Worker constructor
    JSValue worker_ctor = JS_NewCFunction2(ctx, worker_constructor, "Worker", 1,
                                          JS_CFUNC_constructor, 0);

    JS_DefinePropertyValueStr(ctx, global, "Worker", worker_ctor, 0);
    JS_FreeValue(ctx, global);
}

void process_worker_messages(JSContext* ctx) {
    WorkerState* state = static_cast<WorkerState*>(JS_GetContextOpaque(ctx));
    if (!state) {
        return;
    }

    auto it = state->workers.begin();
    while (it != state->workers.end()) {
        std::shared_ptr<WorkerThread> worker = it->second.thread;
        JSValue worker_obj = it->second.js_object;

        // Try to receive a message from the worker
        std::string msg_data = worker->try_recv_message_from_worker();
        if (!msg_data.empty()) {
            // Parse JSON and call onmessage handler
            JSValue json_val = JS_JSONParse(ctx, msg_data.c_str(), msg_data.c_str() + msg_data.size());
            if (!JS_IsException(json_val)) {
                JSValue event_obj = JS_NewObject(ctx);
                JS_DefinePropertyValueStr(ctx, event_obj, "data", json_val, 0);

                JSValue onmessage = JS_GetPropertyStr(ctx, worker_obj, "onmessage");
                if (JS_IsFunction(ctx, onmessage)) {
                    JSValue call_result = JS_Call(ctx, onmessage, worker_obj, 1, &event_obj);
                    if (JS_IsException(call_result)) {
                        JS_FreeValue(ctx, JS_GetException(ctx));
                    }
                    JS_FreeValue(ctx, call_result);
                }
                JS_FreeValue(ctx, onmessage);
                JS_FreeValue(ctx, event_obj);
            } else {
                JS_FreeValue(ctx, JS_GetException(ctx));
            }
        }

        // Clean up finished workers
        if (worker->is_finished()) {
            JS_FreeValue(ctx, worker_obj);
            it = state->workers.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace clever::js
